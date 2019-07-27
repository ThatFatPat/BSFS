#include "bsfs_priv.h"

#include "bft.h"
#include "cluster.h"
#include "disk.h"
#include "keytab.h"
#include "stego.h"
#include <errno.h>
#include <linux/stat.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static size_t min(size_t a, size_t b) {
  return a < b ? a : b;
}

static size_t max(size_t a, size_t b) {
  return a > b ? a : b;
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
  pthread_rwlock_rdlock(&level->metadata_lock);

  int ret = fs_write_bitmap(&level->key, disk, level->bitmap);

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

  if (!slash_loc[1]) {
    // No file name was provided after the directory.
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
  // Split the path into password and name
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
    pthread_rwlock_wrlock(&level->metadata_lock);
  } else {
    pthread_rwlock_rdlock(&level->metadata_lock);
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

static int randomize_disk(bs_disk_t disk) {
  void* disk_data;
  int ret = disk_lock_write(disk, &disk_data);
  if (ret < 0) {
    return ret;
  }
  enc_rand_bytes(disk_data, disk_get_size(disk));
  disk_unlock_write(disk);
  return ret;
}

static int format_metadata(bs_disk_t disk, const stego_key_t* keys,
                           size_t levels) {
  size_t bitmap_size = fs_compute_bitmap_size_from_disk(disk);

  void* zero = calloc(1, max(bitmap_size, BFT_SIZE));
  if (!zero) {
    return -ENOMEM;
  }

  int ret = 0;
  for (size_t i = 0; i < levels; i++) {
    ret = bft_write_table(keys + i, disk, zero);
    if (ret < 0) {
      goto cleanup;
    }

    ret = fs_write_bitmap(keys + i, disk, zero);
    if (ret < 0) {
      goto cleanup;
    }
  }

cleanup:
  free(zero);
  return ret;
}

int bsfs_format(int fd, size_t levels, const char* const* passwords) {
  // Disks take ownership of their file descriptors, so make sure the user's fd
  // isn't closed.
  int dup_fd = dup(fd);
  if (dup_fd < 0) {
    return -errno;
  }

  bs_disk_t disk;
  int ret = disk_create(dup_fd, &disk);
  if (ret < 0) {
    return ret;
  }

  if (!fs_compute_bitmap_size_from_disk(disk)) {
    // The disk is to small for a BSFS filesystem
    ret = -ENOSPC;
    goto cleanup_disk;
  }

  // Generate passwords
  stego_key_t keys[STEGO_USER_LEVEL_COUNT];
  ret = stego_gen_user_keys(keys, levels);
  if (ret < 0) {
    goto cleanup_disk;
  }

  ret = randomize_disk(disk);
  if (ret < 0) {
    goto cleanup_disk;
  }

  // Store passwords in the key table
  for (size_t i = 0; i < levels; i++) {
    ret = keytab_store(disk, i, passwords[i], keys + i);
    if (ret < 0) {
      goto cleanup_disk;
    }
  }

  // Zero out BFTs and Bitmaps
  ret = format_metadata(disk, keys, levels);

cleanup_disk:
  disk_free(disk);
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

static int read_cluster(bs_open_level_t level, void* buf,
                        cluster_offset_t cluster) {
  return fs_read_cluster(&level->key, level->fs->disk, buf, cluster);
}

static int write_cluster(bs_open_level_t level, const void* buf,
                         cluster_offset_t cluster) {
  return fs_write_cluster(&level->key, level->fs->disk, buf, cluster);
}

static size_t count_clusters_from_disk(bs_disk_t disk) {
  return fs_count_clusters(stego_compute_user_level_size(disk_get_size(disk)));
}

static int alloc_cluster(bs_open_level_t level, cluster_offset_t* out) {
  size_t bitmap_bits = count_clusters_from_disk(level->fs->disk);
  return fs_alloc_cluster(level->bitmap, bitmap_bits, out);
}

static int dealloc_cluster(bs_open_level_t level, cluster_offset_t cluster) {
  size_t bitmap_bits = count_clusters_from_disk(level->fs->disk);
  return fs_dealloc_cluster(level->bitmap, bitmap_bits, cluster);
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

  pthread_rwlock_wrlock(&level->metadata_lock);

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

  cluster_offset_t initial_cluster;
  ret = alloc_cluster(level, &initial_cluster);
  if (ret < 0) {
    goto cleanup_after_metadata;
  }

  uint8_t cluster_data[CLUSTER_SIZE] = { 0 };
  fs_set_next_cluster(cluster_data, CLUSTER_OFFSET_EOF);

  ret = write_cluster(level, cluster_data, initial_cluster);
  if (ret < 0) {
    dealloc_cluster(level, initial_cluster);
  }

  bft_entry_t ent;
  ret = bft_entry_init(&ent, name, 0, mode, initial_cluster, 0, 0);
  if (ret < 0) {
    dealloc_cluster(level, initial_cluster);
    goto cleanup_after_metadata;
  }

  ret = bft_write_table_entry(level->bft, &ent, offset);
  if (ret < 0) {
    dealloc_cluster(level, initial_cluster);
  }

  bft_entry_destroy(&ent);

cleanup_after_metadata:
  pthread_rwlock_unlock(&level->metadata_lock);
cleanup:
  free(name);
  free(pass);
  return ret;
}

static void do_dealloc_clusters(bs_open_level_t level,
                                cluster_offset_t* cluster_indices,
                                size_t count) {
  for (size_t i = 0; i < count; i++) {
    // If one of these fails, we still want to free the rest.
    dealloc_cluster(level, cluster_indices[i]);
  }
}

static void dealloc_clusters(bs_open_level_t level,
                             cluster_offset_t* cluster_indices, size_t count) {
  pthread_rwlock_wrlock(&level->metadata_lock);
  do_dealloc_clusters(level, cluster_indices, count);
  pthread_rwlock_unlock(&level->metadata_lock);
}

static int get_cluster_chain(bs_open_level_t level,
                             cluster_offset_t cluster_idx,
                             cluster_offset_t** out_cluster_indices,
                             size_t* out_count) {
  uint8_t cluster[CLUSTER_SIZE];

  size_t capacity = 4;
  size_t count = 0;

  cluster_offset_t* cluster_indices =
      calloc(capacity, sizeof(cluster_offset_t));
  if (!cluster_indices) {
    return -ENOMEM;
  }

  int ret = 0;

  while (cluster_idx != CLUSTER_OFFSET_EOF) {
    if (count >= capacity) {
      capacity *= 2;

      cluster_offset_t* new_cluster_indices =
          realloc(cluster_indices, sizeof(cluster_offset_t) * capacity);
      if (!new_cluster_indices) {
        free(cluster_indices);
        return -ENOMEM;
      }

      cluster_indices = new_cluster_indices;
    }

    cluster_indices[count++] = cluster_idx;

    ret = read_cluster(level, cluster, cluster_idx);
    if (ret < 0) {
      return ret;
    }

    cluster_idx = fs_next_cluster(cluster);
  }

  *out_cluster_indices = cluster_indices;
  *out_count = count;

  return ret;
}

static int dealloc_cluster_chain(bs_open_level_t level,
                                 cluster_offset_t initial_cluster) {
  cluster_offset_t* cluster_indices;
  size_t count;
  int ret = get_cluster_chain(level, initial_cluster, &cluster_indices, &count);
  if (ret < 0) {
    return ret;
  }

  dealloc_clusters(level, cluster_indices, count);

  free(cluster_indices);
  return 0;
}

static int do_unlink_metadata(bs_open_level_t level, bft_offset_t index,
                              cluster_offset_t* inital_cluster) {
  if (bs_oft_has(&level->open_files, index)) {
    return -EBUSY;
  }

  bft_entry_t ent;
  int ret = bft_read_table_entry(level->bft, &ent, index);
  if (ret < 0) {
    return ret;
  }

  *inital_cluster = ent.initial_cluster;

  // Remove BFT entry
  ret = bft_remove_table_entry(level->bft, index);
  bft_entry_destroy(&ent);
  return ret;
}

int bsfs_unlink(bs_bsfs_t fs, const char* path) {
  bs_open_level_t level;
  bft_offset_t index;

  int ret = get_locked_level_and_index(fs, path, true, &level, &index);
  if (ret < 0) {
    return ret;
  }

  cluster_offset_t initial_cluster;

  ret = do_unlink_metadata(level, index, &initial_cluster);
  if (ret < 0) {
    return ret;
  }

  pthread_rwlock_unlock(&level->metadata_lock);

  return dealloc_cluster_chain(level, initial_cluster);
}

int bsfs_open(bs_bsfs_t fs, const char* path, bs_file_t* file) {
  bs_open_level_t level;
  bft_offset_t index;

  int ret = get_locked_level_and_index(fs, path, false, &level, &index);
  if (ret < 0) {
    return ret;
  }

  ret = bs_oft_get(&level->open_files, level, index, file);
  pthread_rwlock_unlock(&level->metadata_lock);
  return ret;
}

int bsfs_release(bs_file_t file) {
  bs_open_level_t level = file->level;
  return bs_oft_release(&level->open_files, file);
}

static int get_size_and_initial_cluster(bs_file_t file, off_t* out_size,
                                        cluster_offset_t* out_initial_cluster) {
  bs_open_level_t level = file->level;
  pthread_rwlock_rdlock(&level->metadata_lock);

  bft_entry_t ent;
  int ret = bft_read_table_entry(level->bft, &ent, file->index);
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
  if (local_off) {
    *local_off = off;
  }
  return 0;
}

/**
 * Iterate over a file's clusters and either read or write them.
 */
static ssize_t do_rw_op(bs_open_level_t level, cluster_offset_t cluster_idx,
                        cluster_offset_t* last_index, off_t local_off,
                        void* buf, size_t size, bool write) {
  uint8_t cluster[CLUSTER_SIZE];

  size_t processed = 0;
  while (processed < size) {
    if (cluster_idx == CLUSTER_OFFSET_EOF) {
      return -EIO;
    }

    if (last_index) {
      *last_index = cluster_idx;
    }

    size_t total_remaining = size - processed;
    size_t cluster_remaining = CLUSTER_DATA_SIZE - local_off;
    size_t cur_size = min(total_remaining, cluster_remaining);

    int ret = read_cluster(level, cluster, cluster_idx);
    if (ret < 0) {
      return ret;
    }

    if (write) {
      memcpy(cluster + local_off, (uint8_t*) buf + processed, cur_size);
      ret = write_cluster(level, cluster, cluster_idx);
      if (ret < 0) {
        return ret;
      }
    } else {
      memcpy((uint8_t*) buf + processed, cluster + local_off, cur_size);
    }

    local_off = 0; // We always operate from offset 0 after the first iteration.
    processed += cur_size;

    cluster_idx = fs_next_cluster(cluster);
  }

  return processed;
}

ssize_t bsfs_read(bs_file_t file, void* buf, size_t size, off_t off) {
  pthread_rwlock_rdlock(&file->lock);

  // Get the initial cluster and size of the file.
  off_t file_size;
  cluster_offset_t initial_cluster;
  ssize_t ret =
      get_size_and_initial_cluster(file, &file_size, &initial_cluster);
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
  ret = do_rw_op(file->level, cluster_idx, NULL, local_off, buf, size, false);

unlock:
  pthread_rwlock_unlock(&file->lock);
  return ret;
}

static int alloc_clusters(bs_open_level_t level, size_t count,
                          cluster_offset_t** out) {
  cluster_offset_t* cluster_indices =
      calloc(count + 1, sizeof(*cluster_indices));

  pthread_rwlock_wrlock(&level->metadata_lock);

  int ret = 0;
  size_t i = 0;
  for (; i < count; i++) {
    ret = alloc_cluster(level, cluster_indices + i);
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
  free(cluster_indices);
  return ret;
}

size_t get_required_cluster_count(off_t file_size) {
  // Divide by `CLUSTER_DATA_SIZE`, but round up.
  return (file_size + CLUSTER_DATA_SIZE - 1) / CLUSTER_DATA_SIZE;
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

  size_t new_cluster_count =
      get_required_cluster_count(actual_size - partial_cluster_remaining);

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
    size_t write_size =
        min(remaining,
            CLUSTER_DATA_SIZE); // The size of data to be written to the current
                                // cluster.

    size_t pad_size = (size_t) off > processed
                          ? min(write_size, off - processed)
                          : 0; // The size to be padded in zeros.

    // Before `off`: pad with zeroes.
    memset(cluster, 0, pad_size);

    if (pad_size < write_size) {
      // After `off`: write data.
      memcpy(cluster + pad_size,
             (const uint8_t*) buf + (processed + pad_size - off),
             write_size - pad_size);
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

/**
 * Update file size in BFT and kill privileges if required.
 */
static int commit_write_to_bft(bs_file_t file, off_t new_size, bool killpriv) {
  bs_open_level_t level = file->level;
  pthread_rwlock_wrlock(&level->metadata_lock);

  bft_entry_t ent;
  int ret = bft_read_table_entry(level->bft, &ent, file->index);
  if (ret < 0) {
    goto unlock;
  }

  if (killpriv) {
    ent.mode &= ~(S_ISUID | S_ISGID);
  }

  ent.size = new_size;

  ret = bft_write_table_entry(level->bft, &ent, file->index);
  bft_entry_destroy(&ent);

unlock:
  pthread_rwlock_unlock(&level->metadata_lock);
  return ret;
}

static off_t get_local_eof_off(off_t size) {
  // If the file ends on a cluster boundary, point into the "next" (nonexistent)
  // cluster.
  return (size - 1) % (off_t) CLUSTER_DATA_SIZE + 1;
}

static int find_eof_cluster(bs_open_level_t level,
                            cluster_offset_t initial_cluster, off_t file_size,
                            cluster_offset_t* eof_cluster) {
  if (!file_size) {
    // The file is empty, but `mknod` always allocates at least 1 cluster.
    *eof_cluster = initial_cluster;
    return 0;
  }
  // Find the cluster containing the last byte.
  return find_cluster(level, initial_cluster, file_size - 1, eof_cluster, NULL);
}

ssize_t bsfs_write(bs_file_t file, const void* buf, size_t size, off_t off) {
  if (!size) {
    return size;
  }

  pthread_rwlock_wrlock(&file->lock);

  off_t file_size;
  cluster_offset_t initial_cluster;
  ssize_t ret =
      get_size_and_initial_cluster(file, &file_size, &initial_cluster);
  if (ret < 0) {
    goto unlock;
  }

  cluster_offset_t eof_cluster;

  // Write portion that overlaps with existing contents
  off_t overlap_off = off;
  size_t overlap_size = size;

  adjust_size_and_off(file_size, &overlap_size, &overlap_off);

  if (overlap_size) {
    cluster_offset_t cluster_idx;
    off_t local_off;
    ret = find_cluster(file->level, initial_cluster, overlap_off, &cluster_idx,
                       &local_off);
    if (ret < 0) {
      goto unlock;
    }

    ret = do_rw_op(file->level, cluster_idx, &eof_cluster, local_off,
                   (void*) buf, overlap_size, true);

    if (ret < 0) {
      goto unlock;
    }

    if (overlap_size == size) {
      // Nothing else to write - just kill privileges.
      goto commit;
    }
  } else {
    ret =
        find_eof_cluster(file->level, initial_cluster, file_size, &eof_cluster);
    if (ret < 0) {
      goto unlock;
    }
  }

  // Write portion that extends the file
  ret = bs_do_write_extend(
      file->level, eof_cluster, get_local_eof_off(file_size),
      (const uint8_t*) buf + overlap_size, size - overlap_size,
      file_size > off ? 0 : off - file_size);

  if (ret < 0) {
    goto unlock;
  }

commit:
  ret = commit_write_to_bft(file, max(file_size, off + size), true);

  if (ret >= 0) {
    ret = size;
  }

unlock:
  pthread_rwlock_unlock(&file->lock);
  return ret;
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

  pthread_rwlock_rdlock(&level->metadata_lock);
  int ret = do_getattr(level, file->index, st);
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

  pthread_rwlock_wrlock(&level->metadata_lock);
  int ret = do_chmod(level, file->index, mode);
  pthread_rwlock_unlock(&level->metadata_lock);

  return ret;
}

int bsfs_truncate(bs_bsfs_t fs, const char* path, off_t size) {
  bs_file_t file;
  int ret = bsfs_open(fs, path, &file);
  if (ret < 0) {
    return ret;
  }

  ret = bsfs_ftruncate(file, size);
  bsfs_release(file);
  return ret;
}

int bsfs_ftruncate(bs_file_t file, off_t new_size) {
  pthread_rwlock_wrlock(&file->lock);

  off_t size;
  cluster_offset_t initial_cluster;
  int ret = get_size_and_initial_cluster(file, &size, &initial_cluster);
  if (ret < 0) {
    goto unlock;
  }

  if (new_size != size) {
    if (new_size > size) {
      // Extend
      cluster_offset_t curr_eof;
      ret = find_eof_cluster(file->level, initial_cluster, size, &curr_eof);
      if (ret < 0) {
        goto unlock;
      }

      ret = bs_do_write_extend(file->level, curr_eof, get_local_eof_off(size),
                               NULL, 0, new_size - size);
    } else if (get_required_cluster_count(new_size) <
               get_required_cluster_count(size)) {
      // Shrink
      cluster_offset_t new_eof;
      ret = find_eof_cluster(file->level, initial_cluster, new_size, &new_eof);
      if (ret < 0) {
        goto unlock;
      }

      uint8_t cluster[CLUSTER_SIZE];
      ret = read_cluster(file->level, cluster, new_eof);
      if (ret < 0) {
        goto unlock;
      }

      ret = dealloc_cluster_chain(file->level, fs_next_cluster(cluster));

      if (ret < 0) {
        goto unlock;
      }

      fs_set_next_cluster(cluster, CLUSTER_OFFSET_EOF);
      ret = write_cluster(file->level, cluster, new_eof);
    }

    if (ret >= 0) {
      // Update size and kill privileges
      ret = commit_write_to_bft(file, new_size, true);
    }
  }

unlock:
  pthread_rwlock_unlock(&file->lock);
  return ret;
}

int bsfs_fallocate(bs_file_t file, off_t off, off_t len) {
  pthread_rwlock_wrlock(&file->lock);

  off_t requested_size = off + len;

  off_t file_size;
  cluster_offset_t initial_cluster;
  int ret = get_size_and_initial_cluster(file, &file_size, &initial_cluster);
  if (ret < 0) {
    goto unlock;
  }

  if (requested_size > file_size) {
    cluster_offset_t eof_cluster;
    ret =
        find_eof_cluster(file->level, initial_cluster, file_size, &eof_cluster);
    if (ret < 0) {
      goto unlock;
    }

    ret = bs_do_write_extend(file->level, eof_cluster,
                             get_local_eof_off(file_size), NULL, 0,
                             requested_size - file_size);
    if (ret < 0) {
      goto unlock;
    }

    ret = commit_write_to_bft(file, requested_size, false);
  }

unlock:
  pthread_rwlock_unlock(&file->lock);
  return ret;
}

static bool get_timestamp(const struct timespec times[2], size_t index,
                          bft_timestamp_t* out) {
  if (!times) {
    *out = time(NULL);
    return true;
  }

  const struct timespec* ret_time = times + index;
  if (ret_time->tv_nsec == UTIME_OMIT) {
    return false;
  }

  if (ret_time->tv_nsec == UTIME_NOW) {
    *out = time(NULL);
    return true;
  }

  *out = ret_time->tv_sec;
  return true;
}

static int do_utimens(bs_open_level_t level, bft_offset_t index,
                      const struct timespec times[2]) {
  bft_timestamp_t atim; // Access time
  bft_timestamp_t mtim; // Modification time
  bool needs_atim = get_timestamp(times, 0, &atim);
  bool needs_mtim = get_timestamp(times, 1, &mtim);

  if (!needs_atim && !needs_mtim) {
    return 0;
  }

  bft_entry_t ent;
  int ret = bft_read_table_entry(level->bft, &ent, index);
  if (ret < 0) {
    return ret;
  }

  if (needs_atim) {
    ent.atim = atim;
  }
  if (needs_mtim) {
    ent.mtim = mtim;
  }

  ret = bft_write_table_entry(level->bft, &ent, index);
  bft_entry_destroy(&ent);

  return ret;
}

int bsfs_utimens(bs_bsfs_t fs, const char* path,
                 const struct timespec times[2]) {
  bs_open_level_t level;
  bft_offset_t index;

  int ret = get_locked_level_and_index(fs, path, true, &level, &index);
  if (ret < 0) {
    return ret;
  }

  ret = do_utimens(level, index, times);

  pthread_rwlock_unlock(&level->metadata_lock);
  return ret;
}

int bsfs_futimens(bs_file_t file, const struct timespec times[2]) {
  bs_open_level_t level = file->level;

  pthread_rwlock_wrlock(&level->metadata_lock);
  int ret = do_utimens(level, file->index, times);
  pthread_rwlock_unlock(&level->metadata_lock);

  return ret;
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

  bool unlink_new = false;
  cluster_offset_t new_initial_cluster;

  pthread_rwlock_wrlock(&level->metadata_lock);

  bft_offset_t old_index;
  ret = bft_find_table_entry(level->bft, old_name, &old_index);
  if (ret < 0) {
    goto unlock;
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
        ret = do_unlink_metadata(level, new_index, &new_initial_cluster);
        if (ret < 0) {
          goto unlock;
        }
        unlink_new = true;
      }
    }

    ret = do_rename(level, old_index, new_name);
  }

unlock:
  pthread_rwlock_unlock(&level->metadata_lock);

  if (unlink_new) {
    ret = dealloc_cluster_chain(level, new_initial_cluster);
  }

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
