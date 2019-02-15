#include "bsfs_priv.h"

#include "stego.h"
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#define OFT_INITIAL_BUCKET_COUNT (1 << 3) // must be power of 2
#define OFT_MAX_LOAD_FACTOR 1.f

static int create_open_file(struct bs_open_level_impl* level,
                            bft_offset_t index, bs_file_t* out) {
  bs_file_t file = (bs_file_t) calloc(1, sizeof(struct bs_file_impl));
  if (!file) {
    return -ENOMEM;
  }

  int ret = -pthread_rwlock_init(&file->lock, NULL);
  if (ret < 0) {
    free(file);
    return ret;
  }

  // note: initial refcount is 0 (not 1), so it must be incremented elsewhere
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
    // not a power of 2
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
  return (size_t) index & (bucket_count - 1); // assumes power-of-2 bucket count
}

static bool oft_matches_bucket(bs_file_t file, size_t bucket,
                               size_t bucket_count) {
  return file && oft_bucket_of(file->index, bucket_count) == bucket;
}

static bs_file_t oft_find(bs_oft_t* table, bft_offset_t index) {
  size_t bucket = oft_bucket_of(index, table->bucket_count);
  bs_file_t* prev_link = table->buckets[bucket];

  if (prev_link) {
    for (bs_file_t iter = *prev_link;
         oft_matches_bucket(iter, bucket, table->bucket_count);
         iter = iter->next) {
      if (iter->index == index) {
        return iter;
      }
    }
  }

  return NULL;
}

static void oft_do_insert(bs_oft_t* table, bs_file_t file) {
  size_t bucket = oft_bucket_of(file->index, table->bucket_count);
  if (table->buckets[bucket]) {
    // insert at head of existing bucket
    bs_file_t* prev_link = table->buckets[bucket];
    file->next = *prev_link;
    *prev_link = file;
  } else {
    // insert at head of table
    file->next = table->head;
    table->head = file;
    table->buckets[bucket] = &table->head;

    if (file->next) {
      // patch other bucket with new next pointer
      size_t next_bucket =
          oft_bucket_of(file->next->index, table->bucket_count);
      table->buckets[next_bucket] = &file->next;
    }
  }
}

static int oft_remove(bs_oft_t* table, bs_file_t file) {
  size_t bucket = oft_bucket_of(file->index, table->bucket_count);
  if (!table->buckets[bucket]) {
    return -EINVAL;
  }

  bs_file_t* prev_link = table->buckets[bucket];
  for (; *prev_link != file &&
         oft_matches_bucket(*prev_link, bucket, table->bucket_count);
       prev_link = &(*prev_link)->next) {
  }

  if (*prev_link != file) {
    return -EINVAL;
  }

  if (*table->buckets[bucket] == file &&
      !oft_matches_bucket(file->next, bucket, table->bucket_count)) {
    // `file` was the only entry in the bucket - empty it
    table->buckets[bucket] = NULL;
  }

  *prev_link = file->next;

  if (*prev_link) {
    // patch other bucket with new next pointer
    size_t next_bucket =
        oft_bucket_of((*prev_link)->index, table->bucket_count);
    table->buckets[next_bucket] = prev_link;
  }

  table->size--;

  return 0;
}

static int oft_rehash(bs_oft_t* table, size_t bucket_count) {
  int ret = oft_realloc_buckets(table, bucket_count);
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
    // note: still power of 2
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
  return -pthread_mutex_init(&table->lock, NULL);
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

int bs_oft_get(bs_oft_t* table, struct bs_open_level_impl* level,
               bft_offset_t index, bs_file_t* file) {
  int ret = -pthread_mutex_lock(&table->lock);
  if (ret < 0) {
    return ret;
  }

  bs_file_t existing_file = oft_find(table, index);
  if (existing_file) {
    *file = existing_file;
    goto success;
  }

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

success:
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

  // recheck refcount under lock (double-checked locking)
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

int bsfs_init(int fd, bs_bsfs_t* fs) {
  return -ENOSYS;
}

void bsfs_destry(bs_bsfs_t fs) {
}

int bsfs_mknod(bs_bsfs_t fs, const char* path) {
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

int readdir(bs_bsfs_t fs, const char* name, bs_dir_iter_t iter) {
  return -ENOSYS;
}
