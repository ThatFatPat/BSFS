#include "bsfs_priv.h"

#include "cluster.h"
#include "keytab.h"
#include "stego.h"
#include <errno.h>
#include <stdbool.h>
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

static void level_destroy(bs_open_level_t level) {
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

int bsfs_init(int fd, bs_bsfs_t* fs) {
  return -ENOSYS;
}

void bsfs_destroy(bs_bsfs_t fs) {
}

int bsfs_mknod(bs_bsfs_t fs, const char* path, mode_t mode) {
  return -ENOSYS;
}

int bsfs_unlink(bs_bsfs_t fs, const char* path) {
  return -ENOSYS;
}

int bsfs_open(bs_bsfs_t fs, const char* path, bs_file_t* file) {
  return -ENOSYS;
}

int bsfs_release(bs_file_t file) {
  return -ENOSYS;
}

ssize_t bsfs_read(bs_file_t file, void* buf, size_t size, off_t off) {
  return -ENOSYS;
}

ssize_t bsfs_write(bs_file_t file, const void* buf, size_t size, off_t off) {
  return -ENOSYS;
}

int bsfs_getattr(bs_bsfs_t fs, const char* path, struct stat* st) {
  return -ENOSYS;
}

int bsfs_setattr(bs_bsfs_t fs, const char* path, const struct stat* st) {
  return -ENOSYS;
}

int bsfs_rename(bs_bsfs_t fs, const char* src_path, const char* new_name,
                unsigned int flags) {
  return -ENOSYS;
}

int bsfs_chmod(bs_bsfs_t fs, const char* path, mode_t mode) {
  return -ENOSYS;
}

int bsfs_truncate(bs_bsfs_t fs, const char* path, off_t size) {
  return -ENOSYS;
}

int bsfs_readdir(bs_bsfs_t fs, const char* name, bs_dir_iter_t iter,
                 void* ctx) {
  return -ENOSYS;
}
