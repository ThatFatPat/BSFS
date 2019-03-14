#include "bsfs_priv.h"

#include "bft.h"
#include "cluster.h"
#include "disk.h"
#include "keytab.h"
#include "stego.h"
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

static size_t count_clusters_from_disk(bs_disk_t disk) {
  return fs_count_clusters(stego_compute_user_level_size(disk_get_size(disk)));
}

static int unlink_with_offset(bs_open_level_t level, bft_offset_t index) {

  // Assumes metadata_lock is locked!!!!
  // TODO: Remove fs and replace instances with level->fs

  bft_entry_t ent;
  int ret = bft_read_table_entry(level->bft, &ent, index);
  if (ret < 0) {
    goto cleanup;
  }
  cluster_offset_t init_cluster_idx = ent.initial_cluster;

  // Remove BFT entry
  ret = bft_remove_table_entry(level->bft, index);
  bft_entry_destroy(&ent);
  if (ret < 0) {
    goto cleanup;
  }

  cluster_offset_t cluster_idx = init_cluster_idx;
  uint8_t cluster[CLUSTER_SIZE];
  size_t bitmap_bits = count_clusters_from_disk(level->fs->disk);

  // Dealloc clusters
  while (cluster_idx != CLUSTER_OFFSET_EOF) {
    ret = fs_dealloc_cluster(level->bitmap, bitmap_bits, cluster_idx);
    if (ret < 0) {
      goto cleanup;
    }

    ret = fs_read_cluster(&level->key, level->fs->disk, cluster, cluster_idx);
    if (ret < 0) {
      goto cleanup;
    }

    cluster_idx = fs_next_cluster(cluster);
  }

cleanup:
  return ret;
}

static int exchange_filenames(bs_open_level_t level, bft_offset_t src,
                              bft_offset_t dest) {
  bft_entry_t src_entry;
  bft_entry_t dest_entry;

  int ret = bft_read_table_entry(&level->bft, &src_entry, src);
  if (ret < 0) {
    return ret;
  }

  ret = bft_read_table_entry(&level->bft, &dest_entry, dest);
  if (ret < 0) {
    goto cleanup;
  }

  char* temp = src_entry.name;
  src_entry.name = dest_entry.name;
  dest_entry.name = temp;

  ret = bft_write_table_entry(&level->bft, &src_entry, src);
  if (ret < 0) {
    goto new_cleanup;
  }

  ret = bft_write_table_entry(&level->bft, &dest_entry, dest);
  if (ret < 0) {
    goto new_cleanup;
  }

new_cleanup:
  free(temp);
  bft_entry_destroy(&dest_entry);
cleanup:
  bft_entry_destroy(&src_entry);
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

  ret = fs_write_cluster(&level->key, fs->disk, cluster_data, initial_cluster);
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

int bsfs_unlink(bs_bsfs_t fs, const char* path) {
  bs_open_level_t level;
  bft_offset_t index;

  int ret = get_locked_level_and_index(fs, path, true, &level, &index);
  if (ret < 0) {
    return ret;
  }

  ret = unlink_with_offset(level, index);
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

ssize_t bsfs_read(bs_file_t file, void* buf, size_t size, off_t off) {
  return -ENOSYS;
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

int bsfs_getattr(bs_bsfs_t fs, const char* path, struct stat* st) {
  return -ENOSYS;
}

int bsfs_fgetattr(bs_file_t file, struct stat* st) {
  return -ENOSYS;
}

int bsfs_setattr(bs_bsfs_t fs, const char* path, const struct stat* st) {
  return -ENOSYS;
}

int bsfs_fsetattr(bs_file_t file, const struct stat* st) {
  return -ENOSYS;
}

int bsfs_chmod(bs_bsfs_t fs, const char* path, mode_t mode) {
  return -ENOSYS;
}

int bsfs_fchmod(bs_file_t file, mode_t mode) {
  return -ENOSYS;
}

int bsfs_truncate(bs_bsfs_t fs, const char* path, off_t size) {
  return -ENOSYS;
}

int bsfs_ftruncate(bs_file_t file, off_t size) {
  return -ENOSYS;
}

// TODO: Extract guts of unlink to enable removal using bft_offset as well as
// path.

int bsfs_rename(bs_bsfs_t fs, const char* src_path, const char* new_path,
                unsigned int flags) {
  bs_open_level_t level;
  bft_offset_t index;

  if (flags & RENAME_WHITEOUT) {
    return -ENOTSUP;
  }

  int ret = get_locked_level_and_index(fs, src_path, false, &level, &index);
  if (ret < 0) {
    return ret;
  }

  bft_offset_t new_path_index;
  bool new_path_exists;
  char* new_name;
  char* new_pass;
  char* name;
  char* pass;

  ret = bs_split_path(new_path, &new_pass, &new_name);
  if (ret < 0) {
    ret = -EINVAL;
    goto cleanup;
  }

  ret = bs_split_path(src_path, &pass, &name);
  if (ret < 0) {
    ret = -ENOENT;
    goto cleanup_after_old_alloc;
  }

  if (strcmp(pass, new_pass)) {
    ret = -EXDEV;
    goto cleanup_after_old_alloc;
  }

  int ret_new_file =
      bft_find_table_entry(level->bft, new_name, &new_path_index);
  if (ret_new_file < 0) {
    new_path_exists = false;
  }

  if (new_path_index) {
    new_path_exists = true;
  }

  // Logic: Implement RENAME_EXCHANGE case here.
  if (flags & RENAME_EXCHANGE) {
    ret = exchange_filenames(level, index, new_path_index);
    goto cleanup_after_old_alloc;
  }

  // Handle NOREPLACE
  if (new_path_exists)
    if (flags & RENAME_NOREPLACE) {
      ret = -EEXIST;
      goto cleanup_after_old_alloc;
    }

  bool is_open;
  ret = bs_oft_has(&level->open_files, index, &is_open);
  if (ret < 0) {
    goto cleanup_after_old_alloc;
  }

  if (is_open) {
    ret = -EBUSY;
    goto cleanup_after_old_alloc;
  }

  bft_entry_t ent;
  ret = bft_read_table_entry(level->bft, &ent, index);
  if (ret < 0) {
    goto cleanup_after_old_alloc;
  }

  ent.name = new_name; // Change name

  if (new_path_exists) {
    unlink_with_offset(level, index);
  }

  ret = bft_write_table_entry(level->bft, &ent, index);
  if (ret < 0) {
    goto cleanup_after_old_alloc;
  }

cleanup_after_old_alloc:
  free(name);
  free(pass);
  free(new_name);
  free(new_pass);
cleanup:
  pthread_rwlock_unlock(&level->metadata_lock);
  return ret;
}

int bsfs_readdir(bs_bsfs_t fs, const char* name, bs_dir_iter_t iter,
                 void* ctx) {
  return -ENOSYS;
}
