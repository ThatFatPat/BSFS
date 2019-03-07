#include "bsfs_priv.h"

#include "cluster.h"
#include "keytab.h"
#include "stego.h"
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#define OFT_INITIAL_BUCKET_COUNT (1 << 3) // Must be a power of 2.
#define OFT_MAX_LOAD_FACTOR 1.f

static int create_open_file(bs_open_level_t level, bft_offset_t index,
                            bs_file_t* out) {
  bs_file_t file = (bs_file_t) calloc(1, sizeof(struct bs_file_impl));
  if (!file) {
    return -ENOMEM;
  }

  int ret = -pthread_rwlock_init(&file->lock, NULL);
  if (ret < 0) {
    free(file);
    return ret;
  }

  // Note: initial refcount is 0 (not 1), so it must be incremented elsewhere.
  file->index = index;
  file->level = level;
  *out = file;

  return 0;
}

static void destroy_open_file(bs_file_t file) {
  pthread_rwlock_destroy(&file->lock);
  free(file);
}

static int oft_realloc_buckets(bs_oft_t* table, size_t bucket_count) {
  if (!bucket_count || bucket_count & (bucket_count - 1)) {
    // Not a power of 2
    return -EINVAL;
  }

  bs_file_t** new_buckets =
      (bs_file_t**) calloc(bucket_count, sizeof(bs_file_t*));
  if (!new_buckets) {
    return -ENOMEM;
  }
  free(table->buckets);
  table->buckets = new_buckets;
  table->bucket_count = bucket_count;
  return 0;
}

static size_t oft_bucket_of(bft_offset_t index, size_t bucket_count) {
  return (size_t) index &
         (bucket_count - 1); // Assumes power-of-2 bucket count.
}

static bool oft_matches_bucket(bs_file_t file, size_t bucket,
                               size_t bucket_count) {
  return file && oft_bucket_of(file->index, bucket_count) == bucket;
}

static bs_file_t* oft_find_prev(bs_oft_t* table, bft_offset_t index) {
  size_t bucket = oft_bucket_of(index, table->bucket_count);
  bs_file_t* prev_link = table->buckets[bucket];

  if (!prev_link) {
    return NULL;
  }

  for (; oft_matches_bucket(*prev_link, bucket, table->bucket_count);
       prev_link = &(*prev_link)->next) {
    if ((*prev_link)->index == index) {
      return prev_link;
    }
  }

  return NULL;
}

static bs_file_t oft_find(bs_oft_t* table, bft_offset_t index) {
  bs_file_t* prev_link = oft_find_prev(table, index);
  return prev_link ? *prev_link : NULL;
}

static void oft_do_insert(bs_oft_t* table, bs_file_t file) {
  size_t bucket = oft_bucket_of(file->index, table->bucket_count);
  if (table->buckets[bucket]) {
    // Insert at head of existing bucket.
    bs_file_t* prev_link = table->buckets[bucket];
    file->next = *prev_link;
    *prev_link = file;
  } else {
    // Insert at head of table.
    file->next = table->head;
    table->head = file;
    table->buckets[bucket] = &table->head;

    if (file->next) {
      // Patch other bucket with new next pointer.
      size_t next_bucket =
          oft_bucket_of(file->next->index, table->bucket_count);
      table->buckets[next_bucket] = &file->next;
    }
  }
}

static int oft_remove(bs_oft_t* table, bs_file_t file) {
  size_t bucket = oft_bucket_of(file->index, table->bucket_count);

  bs_file_t* prev_link = oft_find_prev(table, file->index);
  if (!prev_link) {
    return -EINVAL;
  }

  if (*table->buckets[bucket] == file &&
      !oft_matches_bucket(file->next, bucket, table->bucket_count)) {
    // `file` was the only entry in the bucket - empty it.
    table->buckets[bucket] = NULL;
  }

  *prev_link = file->next;

  if (*prev_link) {
    size_t next_bucket =
        oft_bucket_of((*prev_link)->index, table->bucket_count);

    if (next_bucket != bucket) {
      // The next item is in a different bucket - patch it with new next
      // pointer.
      table->buckets[next_bucket] = prev_link;
    }
  }

  table->size--;

  return 0;
}

static int oft_rehash(bs_oft_t* table, size_t new_bucket_count) {
  int ret = oft_realloc_buckets(table, new_bucket_count);
  if (ret < 0) {
    return ret;
  }

  bs_file_t iter = table->head;
  table->head = NULL;

  while (iter) {
    bs_file_t next = iter->next;
    oft_do_insert(table, iter);
    iter = next;
  }

  return 0;
}

static int oft_insert(bs_oft_t* table, bs_file_t file) {
  if (table->size + 1 > table->bucket_count * OFT_MAX_LOAD_FACTOR) {
    // Note: still a power of 2.
    int rehash_status = oft_rehash(table, 2 * table->bucket_count);
    if (rehash_status < 0) {
      return rehash_status;
    }
  }

  oft_do_insert(table, file);
  table->size++;

  return 0;
}

int bs_oft_init(bs_oft_t* table) {
  memset(table, 0, sizeof(bs_oft_t));
  int ret = oft_realloc_buckets(table, OFT_INITIAL_BUCKET_COUNT);
  if (ret < 0) {
    return ret;
  }

  ret = -pthread_mutex_init(&table->lock, NULL);
  if (ret < 0) {
    free(table->buckets);
  }

  return ret;
}

void bs_oft_destroy(bs_oft_t* table) {
  pthread_mutex_destroy(&table->lock);
  free(table->buckets);

  bs_file_t iter = table->head;
  while (iter) {
    bs_file_t next = iter->next;
    destroy_open_file(iter);
    iter = next;
  }
}

int bs_oft_get(bs_oft_t* table, bs_open_level_t level, bft_offset_t index,
               bs_file_t* file) {
  int ret = -pthread_mutex_lock(&table->lock);
  if (ret < 0) {
    return ret;
  }

  *file = oft_find(table, index);

  if (!*file) {
    bs_file_t new_file;
    ret = create_open_file(level, index, &new_file);
    if (ret < 0) {
      goto unlock;
    }

    ret = oft_insert(table, new_file);
    if (ret < 0) {
      destroy_open_file(new_file);
      goto unlock;
    }

    *file = new_file;
  }

  atomic_fetch_add_explicit(&(*file)->refcount, 1, memory_order_relaxed);

unlock:
  pthread_mutex_unlock(&table->lock);
  return ret;
}

int bs_oft_release(bs_oft_t* table, bs_file_t file) {
  int new_refcount =
      atomic_fetch_sub_explicit(&file->refcount, 1, memory_order_acq_rel) - 1;
  if (new_refcount) {
    return 0;
  }

  int ret = -pthread_mutex_lock(&table->lock);
  if (ret < 0) {
    return ret;
  }

  // Recheck refcount under lock (double-checked locking).
  new_refcount = atomic_load_explicit(&file->refcount, memory_order_acquire);
  if (new_refcount) {
    goto unlock;
  }

  ret = oft_remove(table, file);
  if (ret < 0) {
    goto unlock;
  }

  pthread_mutex_unlock(&table->lock);
  destroy_open_file(file);
  return 0;

unlock:
  pthread_mutex_unlock(&table->lock);
  return ret;
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
