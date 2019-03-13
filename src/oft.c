#include "bsfs_priv.h"

#include <errno.h>
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
  bs_file_t iter = table->buckets[oft_bucket_of(index, table->bucket_count)];
  for (; iter; iter = iter->next) {
    if (iter->index == index) {
      break;
    }
  }
  return iter;
}

static void oft_do_insert(bs_file_t* buckets, size_t bucket_count,
                          bs_file_t file) {
  size_t bucket = oft_bucket_of(file->index, bucket_count);
  file->next = buckets[bucket];
  buckets[bucket] = file;
}

static int oft_remove(bs_oft_t* table, bs_file_t file) {
  size_t bucket = oft_bucket_of(file->index, table->bucket_count);

  bs_file_t* iter = &table->buckets[bucket];
  for (; *iter; iter = &(*iter)->next) {
    if (*iter == file) {
      break;
    }
  }

  if (!*iter) {
    return -EINVAL;
  }

  *iter = (*iter)->next;
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

  oft_do_insert(table->buckets, table->bucket_count, file);
  table->size++;

  return 0;
}

int bs_oft_init(bs_oft_t* table) {
  memset(table, 0, sizeof(bs_oft_t));

  table->buckets = calloc(OFT_INITIAL_BUCKET_COUNT, sizeof(bs_file_t));
  if (!table->buckets) {
    return -ENOMEM;
  }
  table->bucket_count = OFT_INITIAL_BUCKET_COUNT;

  int ret = -pthread_mutex_init(&table->lock, NULL);
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
