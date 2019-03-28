#include "bsfs_priv.h"

#include "bft.h"
#include "cluster.h"
#include "disk.h"
#include "keytab.h"
#include "stego.h"
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

static size_t min(size_t a, size_t b) {
  return a < b ? a : b;
}

/**
 * Initialize an open level
 */
static int level_init(bs_open_level_t level, bs_bsfs_t fs, stego_key_t* key,
                      const char* pass) {
  int ret = 0;

  void* bft = NULL;
  void* bitmap = NULL;

  // Initialize the bft.
  bft = malloc(BFT_SIZE);
  if (!bft) {
    return -ENOMEM;
  }

  ret = bft_read_table(key, fs->disk, bft);
  if (ret < 0) {
    goto fail_after_allocs;
  }

  // Initialize the bitmap.
  bitmap = malloc(fs_compute_bitmap_size_from_disk(fs->disk));
  if (!bitmap) {
    ret = -ENOMEM;
    goto fail_after_allocs;
  }

  ret = fs_read_bitmap(key, fs->disk, bitmap);
  if (ret < 0) {
    goto fail_after_allocs;
  }

  // Initialize the open file table.
  ret = bs_oft_init(&level->open_files);
  if (ret < 0) {
    goto fail_after_allocs;
  }

  // Initialize the rwlock for metadata.
  ret = -pthread_rwlock_init(&level->metadata_lock, NULL);
  if (ret < 0) {
    goto fail_after_oft;
  }

  // Set the level's parameters
  level->pass = strdup(pass);
  if (!level->pass) {
    ret = -ENOMEM;
    goto fail_after_lock;
  }
  level->key = *key;
  level->bft = bft;
  level->bitmap = bitmap;

  // This must be last, as setting `fs` also marks the level as in-use.
  level->fs = fs;
  return 0;

fail_after_lock:
  pthread_rwlock_destroy(&level->metadata_lock);
fail_after_oft:
  bs_oft_destroy(&level->open_files);
fail_after_allocs:
  free(bitmap);
  free(bft);
  return ret;
}

static int level_flush_metadata(bs_open_level_t level) {
  bs_disk_t disk = level->fs->disk;

  // Note: take a read lock here as we are not modifying the in-memory cache.
  int ret = -pthread_rwlock_rdlock(&level->metadata_lock);
  if (ret < 0) {
    return ret;
  }

  ret = fs_write_bitmap(&level->key, disk, level->bitmap);

  if (ret >= 0) {
    ret = bft_write_table(&level->key, disk, level->bft);
  }

  pthread_rwlock_unlock(&level->metadata_lock);
  return ret;
}

static void level_destroy(bs_open_level_t level) {
  level_flush_metadata(level);
  free(level->bft);
  free(level->bitmap);
  free(level->pass);
  pthread_rwlock_destroy(&level->metadata_lock);
  bs_oft_destroy(&level->open_files);
  memset(level, 0, sizeof(*level));
}

static int level_open(bs_bsfs_t fs, const char* pass, size_t index,
                      bs_open_level_t* out) {
  // Get the key
  stego_key_t key;
  int ret = keytab_lookup(fs->disk, pass, &key);
  if (ret < 0) {
    return ret;
  }

  // Initialize the level
  bs_open_level_t level = fs->levels + index;
  ret = level_init(level, fs, &key, pass);
  if (ret < 0) {
    return ret;
  }

  *out = level;
  return 0;
}

int bs_level_get(bs_bsfs_t fs, const char* pass, bs_open_level_t* out) {
  pthread_mutex_lock(&fs->level_lock);

  int ret = 0;
  ssize_t free_idx = -1;

  for (size_t i = 0; i < STEGO_USER_LEVEL_COUNT; i++) {
    bs_open_level_t level = &fs->levels[i];

    // The level is open
    if (level->pass && !strcmp(level->pass, pass)) {
      *out = level;
      goto unlock;
    }

    // Simultaneously keep track of a free level, to avoid another search later.
    if (!level->fs) {
      free_idx = i;
    }
  }

  if (free_idx == -1) {
    // All levels are already in use, so whichever level the user is trying to
    // open necessarily doesn't exist.
    ret = -ENOENT;
    goto unlock;
  }

  ret = level_open(fs, pass, free_idx, out);

unlock:
  pthread_mutex_unlock(&fs->level_lock);
  return ret;
}

int bs_get_dirname(const char* path, char** out_pass) {
  if (*path == '/') {
    path++;
  }

  char* slash_loc = strchr(path, '/');
  size_t end;
  if (slash_loc) {
    if (slash_loc[1]) {
      return -ENOTDIR;
    }
    end = slash_loc - path;
  } else {
    end = strlen(path);
  }

  char* pass = strndup(path, end);
  if (!pass) {
    return -errno;
  }

  *out_pass = pass;
  return 0;
}

int bs_split_path(const char* path, char** out_pass, char** out_name) {
  if (*path == '/') {
    path++;
  }

  char* slash_loc = strchr(path, '/');
  if (!slash_loc) {
    return -ENOTSUP;
  }

  if (strchr(slash_loc + 1, '/')) {
    // Another slash was found in the path, but subdirectories are not
    // supported.
    return -ENOTSUP;
  }

  char* pass = strndup(path, slash_loc - path);
  if (!pass) {
    return -ENOMEM;
  }

  char* name = strdup(slash_loc + 1);
  if (!name) {
    free(pass);
    return -ENOMEM;
  }

  *out_pass = pass;
  *out_name = name;
  return 0;
}

static int get_locked_level_and_index(bs_bsfs_t fs, const char* path,
                                      bool write, bs_open_level_t* out_level,
                                      bft_offset_t* out_index) {
  // Split the path to password and name
  char* pass;
  char* name;
  int ret = bs_split_path(path, &pass, &name);
  if (ret < 0) {
    return ret;
  }

  // Find level
  bs_open_level_t level;
  ret = bs_level_get(fs, pass, &level);
  if (ret < 0) {
    goto cleanup;
  }

  // Find BFT index
  if (write) {
    ret = -pthread_rwlock_wrlock(&level->metadata_lock);
  } else {
    ret = -pthread_rwlock_rdlock(&level->metadata_lock);
  }

  if (ret < 0) {
    goto cleanup;
  }

  ret = bft_find_table_entry(level->bft, name, out_index);
  if (ret < 0) {
    pthread_rwlock_unlock(&level->metadata_lock);
  }
  *out_level = level;

  // Note: on success, the level remains locked!

cleanup:
  free(name);
  free(pass);
  return ret;
}

int bsfs_init(int fd, bs_bsfs_t* out) {
  bs_bsfs_t fs = calloc(1, sizeof(*fs));
  if (!fs) {
    return -ENOMEM;
  }

  // Initialize lock
  int ret = -pthread_mutex_init(&fs->level_lock, NULL);
  if (ret < 0) {
    goto fail_after_alloc;
  }

  // Initialize disk
  ret = disk_create(fd, &fs->disk);
  if (ret < 0) {
    goto fail_after_mutex;
  }

  if (!fs_compute_bitmap_size_from_disk(fs->disk)) {
    // The disk is too small to contain a valid filesystem
    ret = -ENOSPC;
    goto fail_after_mutex;
  }

  *out = fs;
  return 0;

fail_after_mutex:
  pthread_mutex_destroy(&fs->level_lock);
fail_after_alloc:
  free(fs);
  return ret;
}

void bsfs_destroy(bs_bsfs_t fs) {
  // Destroy all open levels
  for (size_t i = 0; i < STEGO_USER_LEVEL_COUNT; i++) {
    bs_open_level_t level = &fs->levels[i];
    if (level->fs) {
      // Level is in use
      level_destroy(level);
    }
  }

  pthread_mutex_destroy(&fs->level_lock);
  disk_free(fs->disk);
  free(fs);
}

static size_t count_clusters_from_disk(bs_disk_t disk) {
  return fs_count_clusters(stego_compute_user_level_size(disk_get_size(disk)));
}

static int read_cluster(bs_open_level_t level, void* buf,
                        cluster_offset_t cluster) {
  return fs_read_cluster(&level->key, level->fs->disk, buf, cluster);
}

static int write_cluster(bs_open_level_t level, const void* buf,
                         cluster_offset_t cluster) {
  return fs_write_cluster(&level->key, level->fs->disk, buf, cluster);
}

int bsfs_mknod(bs_bsfs_t fs, const char* path, mode_t mode) {
  if (!S_ISREG(mode)) {
    return -ENOTSUP;
  }

  char* pass;
  char* name;
  int ret = bs_split_path(path, &pass, &name);
  if (ret < 0) {
    return ret;
  }

  // Find level
  bs_open_level_t level;
  ret = bs_level_get(fs, pass, &level);
  if (ret < 0) {
    goto cleanup;
  }

  ret = -pthread_rwlock_wrlock(&level->metadata_lock);
  if (ret < 0) {
    goto cleanup;
  }

  // Check if file exists
  bft_offset_t existing_ent_index;
  if (!bft_find_table_entry(level->bft, name, &existing_ent_index)) {
    ret = -EEXIST;
    goto cleanup_after_metadata;
  }

  bft_offset_t offset;
  ret = bft_find_free_table_entry(level->bft, &offset);
  if (ret < 0) {
    goto cleanup_after_metadata;
  }

  size_t bitmap_bits = count_clusters_from_disk(fs->disk);

  cluster_offset_t initial_cluster;
  ret = fs_alloc_cluster(level->bitmap, bitmap_bits, &initial_cluster);
  if (ret < 0) {
    goto cleanup_after_metadata;
  }

  uint8_t cluster_data[CLUSTER_SIZE] = { 0 };
  fs_set_next_cluster(cluster_data, CLUSTER_OFFSET_EOF);

  ret = write_cluster(level, cluster_data, initial_cluster);
  if (ret < 0) {
    fs_dealloc_cluster(level->bitmap, bitmap_bits, initial_cluster);
  }

  bft_entry_t ent;
  ret = bft_entry_init(&ent, name, 0, mode, initial_cluster, 0, 0);
  if (ret < 0) {
    fs_dealloc_cluster(level->bitmap, bitmap_bits, initial_cluster);
    goto cleanup_after_metadata;
  }

  ret = bft_write_table_entry(level->bft, &ent, offset);
  if (ret < 0) {
    fs_dealloc_cluster(level->bitmap, bitmap_bits, initial_cluster);
  }

  bft_entry_destroy(&ent);

cleanup_after_metadata:
  pthread_rwlock_unlock(&level->metadata_lock);
cleanup:
  free(name);
  free(pass);
  return ret;
}

static int do_unlink(bs_open_level_t level, bft_offset_t index) {
  bft_entry_t ent;
  int ret = bft_read_table_entry(level->bft, &ent, index);
  if (ret < 0) {
    return ret;
  }
  cluster_offset_t init_cluster_idx = ent.initial_cluster;

  // Remove BFT entry
  ret = bft_remove_table_entry(level->bft, index);
  bft_entry_destroy(&ent);
  if (ret < 0) {
    return ret;
  }

  cluster_offset_t cluster_idx = init_cluster_idx;
  uint8_t cluster[CLUSTER_SIZE];
  size_t bitmap_bits = count_clusters_from_disk(level->fs->disk);

  // Dealloc clusters
  while (cluster_idx != CLUSTER_OFFSET_EOF) {
    ret = fs_dealloc_cluster(level->bitmap, bitmap_bits, cluster_idx);
    if (ret < 0) {
      return ret;
    }

    ret = read_cluster(level, cluster, cluster_idx);
    if (ret < 0) {
      return ret;
    }

    cluster_idx = fs_next_cluster(cluster);
  }

  return ret;
}

int bsfs_unlink(bs_bsfs_t fs, const char* path) {
  bs_open_level_t level;
  bft_offset_t index;

  int ret = get_locked_level_and_index(fs, path, true, &level, &index);
  if (ret < 0) {
    return ret;
  }

  ret = do_unlink(level, index);
  pthread_rwlock_unlock(&level->metadata_lock);
  return ret;
}

int bsfs_open(bs_bsfs_t fs, const char* path, bs_file_t* file) {
  bs_open_level_t level;
  bft_offset_t index;

  int ret = get_locked_level_and_index(fs, path, false, &level, &index);
  if (ret < 0) {
    return ret;
  }
  pthread_rwlock_unlock(&level->metadata_lock);

  return bs_oft_get(&level->open_files, level, index, file);
}

int bsfs_release(bs_file_t file) {
  bs_open_level_t level = file->level;
  return bs_oft_release(&level->open_files, file);
}

static int get_size_and_initial_cluster(bs_file_t file, off_t* out_size,
                                        cluster_offset_t* out_initial_cluster) {
  bs_open_level_t level = file->level;
  int ret = -pthread_rwlock_rdlock(&level->metadata_lock);
  if (ret < 0) {
    return ret;
  }

  bft_entry_t ent;
  ret = bft_read_table_entry(level->bft, &ent, file->index);
  if (ret < 0) {
    goto unlock;
  }

  *out_size = ent.size;
  *out_initial_cluster = ent.initial_cluster;

  bft_entry_destroy(&ent);

unlock:
  pthread_rwlock_unlock(&level->metadata_lock);
  return ret;
}

/**
 * Adjust `size` and `off` such that they fit within `file_size`
 */
static void adjust_size_and_off(off_t file_size, size_t* size, off_t* off) {
  if (*off > file_size) {
    *off = file_size;
  }

  if (*size > (size_t)(file_size - *off)) {
    *size = file_size - *off;
  }
}

static int find_cluster(bs_open_level_t level, cluster_offset_t cluster_idx,
                        off_t off, cluster_offset_t* found, off_t* local_off) {
  uint8_t cluster[CLUSTER_SIZE];
  while ((size_t) off >= CLUSTER_DATA_SIZE) {
    int ret = read_cluster(level, cluster, cluster_idx);
    if (ret < 0) {
      return ret;
    }

    cluster_idx = fs_next_cluster(cluster);
    if (cluster_idx == CLUSTER_OFFSET_EOF) {
      return -EIO;
    }

    off -= CLUSTER_DATA_SIZE;
  }

  *found = cluster_idx;
  *local_off = off;
  return 0;
}

typedef void (*file_op_t)(void* buf, size_t buf_size, void* user_buf);

/**
 * Iterate over a file's clusters and perform an operation on them.
 */
static ssize_t do_file_op(file_op_t op, bs_open_level_t level,
                          cluster_offset_t cluster_idx, off_t local_off,
                          void* buf, size_t size) {
  uint8_t cluster[CLUSTER_SIZE];

  size_t processed = 0;
  while (processed < size) {
    size_t total_remaining = size - processed;
    size_t cluster_remaining = CLUSTER_DATA_SIZE - local_off;
    size_t cur_size = min(total_remaining, cluster_remaining);

    int ret = read_cluster(level, cluster, cluster_idx);
    if (ret < 0) {
      return ret;
    }

    op(cluster + local_off, cur_size, (uint8_t*) buf + processed);

    local_off = 0; // We always operate from offset 0 after the first iteration.
    processed += cur_size;

    cluster_idx = fs_next_cluster(cluster);
    if (cluster_idx == CLUSTER_OFFSET_EOF) {
      return -EIO;
    }
  }

  return processed;
}

static void read_op(void* buf, size_t buf_size, void* user_buf) {
  memcpy(user_buf, buf, buf_size);
}

ssize_t bsfs_read(bs_file_t file, void* buf, size_t size, off_t off) {
  ssize_t ret = -pthread_rwlock_rdlock(&file->lock);
  if (ret < 0) {
    return ret;
  }

  // Get the initial cluster and size of the file.
  off_t file_size;
  cluster_offset_t initial_cluster;
  ret = get_size_and_initial_cluster(file, &file_size, &initial_cluster);
  if (ret < 0) {
    goto unlock;
  }

  // Adjust `size` and `off` such that they fit within `file_size`.
  adjust_size_and_off(file_size, &size, &off);
  if (!size) {
    ret = size;
    goto unlock;
  }

  // Find the cluster from which we must start reading.
  cluster_offset_t cluster_idx;
  off_t local_off;
  ret =
      find_cluster(file->level, initial_cluster, off, &cluster_idx, &local_off);
  if (ret < 0) {
    goto unlock;
  }

  // Perform the read.
  ret = do_file_op(read_op, file->level, cluster_idx, local_off, buf, size);

unlock:
  pthread_rwlock_unlock(&file->lock);
  return ret;
}

static void do_dealloc_clusters(bs_open_level_t level,
                                cluster_offset_t* cluster_indices,
                                size_t count) {
  size_t bitmap_bits = count_clusters_from_disk(level->fs->disk);
  for (size_t i = 0; i < count; i++) {
    // If one of these fails, we still want to free the rest.
    fs_dealloc_cluster(level->bitmap, bitmap_bits, cluster_indices[i]);
  }
}

static int alloc_clusters(bs_open_level_t level, size_t count,
                          cluster_offset_t** out) {
  cluster_offset_t* cluster_indices =
      calloc(count + 1, sizeof(*cluster_indices));

  size_t bitmap_bits = count_clusters_from_disk(level->fs->disk);

  int ret = -pthread_rwlock_wrlock(&level->metadata_lock);
  if (ret < 0) {
    goto fail_after_alloc;
  }

  size_t i = 0;
  for (; i < count; i++) {
    ret = fs_alloc_cluster(level->bitmap, bitmap_bits, cluster_indices + i);
    if (ret < 0) {
      goto fail_after_lock;
    }
  }

  pthread_rwlock_unlock(&level->metadata_lock);

  cluster_indices[count] = CLUSTER_OFFSET_EOF;
  *out = cluster_indices;

  return 0;

fail_after_lock:
  if (i) {
    do_dealloc_clusters(level, cluster_indices, i - 1);
  }
  pthread_rwlock_unlock(&level->metadata_lock);
fail_after_alloc:
  free(cluster_indices);
  return ret;
}

static int dealloc_clusters(bs_open_level_t level,
                            cluster_offset_t* cluster_indices, size_t count) {
  int ret = -pthread_rwlock_wrlock(&level->metadata_lock);
  if (ret < 0) {
    return ret;
  }

  do_dealloc_clusters(level, cluster_indices, count);

  pthread_rwlock_unlock(&level->metadata_lock);
  return 0;
}

/**
 * Append the contents of `buf` at offset `off` past the end of the file (as
 * specified by `cluster_idx` and `local_eof_off`).
 */
int bs_do_write_extend(bs_open_level_t level, cluster_offset_t cluster_idx,
                       off_t local_eof_off, const void* buf, size_t buf_size,
                       off_t off) {
  size_t actual_size = buf_size + off;
  size_t partial_cluster_remaining = CLUSTER_DATA_SIZE - local_eof_off;

  // The size of the extra data to be written (past existing cluster) over
  // `CLUSTER_DATA_SIZE` (rounded up).
  size_t new_cluster_count =
      (actual_size - partial_cluster_remaining + CLUSTER_DATA_SIZE - 1) /
      CLUSTER_DATA_SIZE;

  // Allocate new clusters
  cluster_offset_t* new_cluster_indices;
  int ret = alloc_clusters(level, new_cluster_count, &new_cluster_indices);
  if (ret < 0) {
    return ret;
  }

  // Write the new data
  uint8_t cluster[CLUSTER_SIZE];
  size_t processed = partial_cluster_remaining; // Skip partial cluster for now.
  for (size_t i = 0; i < new_cluster_count; i++) {
    size_t remaining = actual_size - processed;
    size_t write_size = min(remaining, CLUSTER_DATA_SIZE);

    if (processed < (size_t) off) {
      // Before `off`: pad with zeroes.
      memset(cluster, 0, write_size);
    } else {
      // After `off`: write data.
      memcpy(cluster, (const uint8_t*) buf + processed - off, write_size);
    }
    processed += write_size;

    fs_set_next_cluster(cluster, new_cluster_indices[i + 1]);
    ret = write_cluster(level, cluster, new_cluster_indices[i]);
    if (ret < 0) {
      goto fail;
    }
  }

  // Hook up to existing cluster chain
  ret = read_cluster(level, cluster, cluster_idx);
  if (ret < 0) {
    goto fail;
  }

  if (off > 0) {
    // Zero-pad if necessary.
    memset(cluster + local_eof_off, 0, min(off, partial_cluster_remaining));
  }

  if ((size_t) off < partial_cluster_remaining) {
    // Possibly write start of `buf` into existing cluster.
    memcpy(cluster + off + local_eof_off, buf,
           min(partial_cluster_remaining - off, buf_size));
  }

  fs_set_next_cluster(cluster, new_cluster_indices[0]);

  ret = write_cluster(level, cluster, cluster_idx);
  if (ret >= 0) {
    free(new_cluster_indices);
    return ret;
  }

fail:
  dealloc_clusters(level, new_cluster_indices, new_cluster_count);
  free(new_cluster_indices);
  return ret;
}

ssize_t bsfs_write(bs_file_t file, const void* buf, size_t size, off_t off) {
  return -ENOSYS;
}

int bsfs_fsync(bs_file_t file, bool datasync) {
  if (!datasync) {
    return level_flush_metadata(file->level);
  }
  return 0;
}

static void stat_from_bft_ent(struct stat* st, const bft_entry_t* ent) {
  *st = (struct stat){ .st_nlink = 1,
                       .st_mode = ent->mode,
                       .st_size = ent->size,
                       .st_atim.tv_sec = ent->atim,
                       .st_mtim.tv_sec = ent->mtim };
}

static int do_getattr(bs_open_level_t level, bft_offset_t index,
                      struct stat* st) {
  bft_entry_t ent;
  int ret = bft_read_table_entry(level->bft, &ent, index);
  if (ret < 0) {
    return ret;
  }
  stat_from_bft_ent(st, &ent);
  bft_entry_destroy(&ent);
  return 0;
}

int bsfs_getattr(bs_bsfs_t fs, const char* path, struct stat* st) {
  char* pass;
  int get_dir_ret = bs_get_dirname(path, &pass);
  if (get_dir_ret < 0 && get_dir_ret != -ENOTDIR) {
    return get_dir_ret;
  }

  if (get_dir_ret == -ENOTDIR) {
    bs_open_level_t level;
    bft_offset_t index;

    int ret = get_locked_level_and_index(fs, path, false, &level, &index);
    if (ret < 0) {
      return ret;
    }

    ret = do_getattr(level, index, st);

    pthread_rwlock_unlock(&level->metadata_lock);
    return ret;
  } else {
    bs_open_level_t level;
    if (*pass) {
      int ret = bs_level_get(fs, pass, &level);
      if (ret < 0) {
        return ret;
      }
    }

    *st = (struct stat){ .st_nlink = 1, .st_mode = S_IFDIR | 0777 };
    return 0;
  }
}

int bsfs_fgetattr(bs_file_t file, struct stat* st) {
  bs_open_level_t level = file->level;

  int ret = -pthread_rwlock_rdlock(&level->metadata_lock);
  if (ret < 0) {
    return ret;
  }

  ret = do_getattr(level, file->index, st);

  pthread_rwlock_unlock(&level->metadata_lock);
  return ret;
}

static int do_chmod(bs_open_level_t level, bft_offset_t index, mode_t mode) {
  bft_entry_t ent;
  int ret = bft_read_table_entry(level->bft, &ent, index);
  if (ret < 0) {
    return ret;
  }

  ent.mode = (mode & 07777) | S_IFREG;

  ret = bft_write_table_entry(level->bft, &ent, index);
  bft_entry_destroy(&ent);

  return ret;
}

int bsfs_chmod(bs_bsfs_t fs, const char* path, mode_t mode) {
  bs_open_level_t level;
  bft_offset_t index;

  int ret = get_locked_level_and_index(fs, path, true, &level, &index);
  if (ret < 0) {
    return ret;
  }

  ret = do_chmod(level, index, mode);

  pthread_rwlock_unlock(&level->metadata_lock);
  return ret;
}

int bsfs_fchmod(bs_file_t file, mode_t mode) {
  bs_open_level_t level = file->level;

  int ret = pthread_rwlock_wrlock(&level->metadata_lock);
  if (ret < 0) {
    return ret;
  }

  ret = do_chmod(level, file->index, mode);

  pthread_rwlock_unlock(&level->metadata_lock);
  return ret;
}

int bsfs_truncate(bs_bsfs_t fs, const char* path, off_t size) {
  return -ENOSYS;
}

int bsfs_ftruncate(bs_file_t file, off_t size) {
  return -ENOSYS;
}

static int do_exchange(bs_open_level_t level, bft_offset_t src,
                       bft_offset_t dest) {
  bft_entry_t src_entry;
  bft_entry_t dest_entry;

  int ret = bft_read_table_entry(level->bft, &src_entry, src);
  if (ret < 0) {
    return ret;
  }

  ret = bft_read_table_entry(level->bft, &dest_entry, dest);
  if (ret < 0) {
    goto cleanup_src;
  }

  const char* temp = src_entry.name;
  src_entry.name = dest_entry.name;
  dest_entry.name = temp;

  ret = bft_write_table_entry(level->bft, &src_entry, src);
  if (ret >= 0) {
    ret = bft_write_table_entry(level->bft, &dest_entry, dest);
  }

  bft_entry_destroy(&dest_entry);

cleanup_src:
  bft_entry_destroy(&src_entry);
  return ret;
}

static int do_rename(bs_open_level_t level, bft_offset_t index,
                     const char* new_name) {
  bft_entry_t ent;
  int ret = bft_read_table_entry(level->bft, &ent, index);
  if (ret < 0) {
    return ret;
  }

  // Manually destroy old name and set new name.
  free((void*) ent.name);
  ent.name = new_name;

  ret = bft_write_table_entry(level->bft, &ent, index);

  // Note: no bft_entry_destroy here, as the name isn't owned by the entry.

  return ret;
}

int bsfs_rename(bs_bsfs_t fs, const char* old_path, const char* new_path,
                unsigned int flags) {
  if (flags && flags != RENAME_NOREPLACE && flags != RENAME_EXCHANGE) {
    return -EINVAL;
  }

  char* new_name = NULL;
  char* new_pass = NULL;
  char* old_name = NULL;
  char* old_pass = NULL;

  int ret = bs_split_path(new_path, &new_pass, &new_name);
  if (ret < 0) {
    return ret;
  }

  ret = bs_split_path(old_path, &old_pass, &old_name);
  if (ret < 0) {
    goto cleanup_after_alloc;
  }

  if (strcmp(old_pass, new_pass)) {
    ret = -EXDEV;
    goto cleanup_after_alloc;
  }

  bs_open_level_t level;
  ret = bs_level_get(fs, old_pass, &level);
  if (ret < 0) {
    goto cleanup_after_alloc;
  }

  ret = -pthread_rwlock_wrlock(&level->metadata_lock);
  if (ret < 0) {
    goto cleanup_after_alloc;
  }

  bft_offset_t old_index;
  ret = bft_find_table_entry(level->bft, old_name, &old_index);
  if (ret < 0) {
    goto cleanup_after_alloc;
  }

  bft_offset_t new_index;
  int ret_new_index = bft_find_table_entry(level->bft, new_name, &new_index);
  if (ret_new_index < 0 && ret_new_index != -ENOENT) {
    ret = ret_new_index;
    goto unlock;
  }

  bool new_exists = !ret_new_index;

  if (flags == RENAME_EXCHANGE) {
    if (!new_exists) {
      ret = -ENOENT;
      goto unlock;
    }

    ret = do_exchange(level, old_index, new_index);
  } else {
    if (new_exists) {
      if (flags == RENAME_NOREPLACE) {
        ret = -EEXIST;
        goto unlock;
      } else {
        do_unlink(level, new_index);
      }
    }

    ret = do_rename(level, old_index, new_name);
  }

unlock:
  pthread_rwlock_unlock(&level->metadata_lock);
cleanup_after_alloc:
  free(old_name);
  free(old_pass);
  free(new_name);
  free(new_pass);
  return ret;
}

struct readdir_ctx {
  bs_dir_iter_t user_iter;
  void* user_ctx;
};

static bool bft_readdir_iter(bft_offset_t off, const bft_entry_t* ent,
                             void* raw_ctx) {
  (void) off;

  struct stat st;
  stat_from_bft_ent(&st, ent);

  struct readdir_ctx* ctx = (struct readdir_ctx*) raw_ctx;
  return ctx->user_iter(ent->name, &st, ctx->user_ctx);
}

int bsfs_readdir(bs_bsfs_t fs, const char* path, bs_dir_iter_t iter,
                 void* user_ctx) {
  char* pass;
  int ret = bs_get_dirname(path, &pass);
  if (ret < 0) {
    return ret;
  }

  bs_open_level_t level = NULL;
  if (*pass) {
    ret = bs_level_get(fs, pass, &level);
    free(pass);
    if (ret < 0) {
      return ret;
    }
  }

  if (!iter(".", NULL, user_ctx)) {
    return 0;
  }

  if (!iter("..", NULL, user_ctx)) {
    return 0;
  }

  if (!level) {
    return 0;
  }

  pthread_rwlock_rdlock(&level->metadata_lock);

  struct readdir_ctx ctx = { .user_iter = iter, .user_ctx = user_ctx };
  ret = bft_iter_table_entries(level->bft, bft_readdir_iter, &ctx);

  pthread_rwlock_unlock(&level->metadata_lock);
  return ret;
}
