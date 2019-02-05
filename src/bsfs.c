#include "bsfs_priv.h"

#include "stego.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#define FTAB_INITIAL_BUCKET_COUNT (1 << 3) // must be power of 2
#define FTAB_MAX_LOAD_FACTOR 1.f

static int create_open_file(struct bs_open_level_impl* level,
                            bft_offset_t index, bs_file_t* out) {
  bs_file_t file = (bs_file_t) malloc(sizeof(struct bs_file_impl));
  if (!file) {
    return -ENOMEM;
  }

  int ret = -pthread_rwlock_init(&file->lock, NULL);
  if (ret < 0) {
    free(file);
    return ret;
  }

  file->index = index;
  file->level = level;
  *out = file;

  return 0;
}

static void destroy_open_file(bs_file_t file) {
  pthread_rwlock_destroy(&file->lock);
  free(file);
}

static int realloc_buckets(bs_file_table_t* table, size_t bucket_count) {
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

static size_t bucket_of(bft_offset_t index, size_t bucket_count) {
  return (size_t) index & (bucket_count - 1); // assumes power-of-2 bucket count
}

static bs_file_t find_open_file(bs_file_table_t* table, bft_offset_t index) {
  size_t bucket = bucket_of(index, table->bucket_count);
  bs_file_t* prev_link = table->buckets[bucket];

  if (prev_link) {
    for (bs_file_t iter = *prev_link;
         iter && bucket_of(iter->index, table->bucket_count) == bucket;
         iter = iter->next) {
      if (iter->index == index) {
        return iter;
      }
    }
  }

  return NULL;
}

static void insert_open_file(bs_file_table_t* table, bs_file_t file) {
  size_t bucket = bucket_of(file->index, table->bucket_count);
  if (table->buckets[bucket]) {
    // insert after existing node
    bs_file_t* prev_link = table->buckets[bucket];
    file->next = *prev_link;
    *prev_link = file;
  } else {
    // insert at head
    file->next = table->head;
    table->head = file;
    table->buckets[bucket] = &table->head;

    if (file->next) {
      // patch other bucket with new next pointer
      size_t next_bucket = bucket_of(file->next->index, table->bucket_count);
      table->buckets[next_bucket] = &file->next;
    }
  }
}

static int rehash_open_files(bs_file_table_t* table, size_t bucket_count) {
  int ret = realloc_buckets(table, bucket_count);
  if (ret < 0) {
    return ret;
  }

  bs_file_t iter = table->head;
  table->head = NULL;

  while (iter) {
    bs_file_t next = iter->next;
    insert_open_file(table, iter);
    iter = next;
  }

  return 0;
}

int bs_file_table_init(bs_file_table_t* table) {
  memset(table, 0, sizeof(bs_file_table_t));
  int ret = realloc_buckets(table, FTAB_INITIAL_BUCKET_COUNT);
  if (ret < 0) {
    return ret;
  }
  return -pthread_mutex_init(&table->lock, NULL);
}

void bs_file_table_destroy(bs_file_table_t* table) {
  pthread_mutex_destroy(&table->lock);
  free(table->buckets);

  bs_file_t iter = table->head;
  while (iter) {
    bs_file_t next = iter->next;
    destroy_open_file(iter);
    iter = next;
  }
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

ssize_t bsfs_read(bs_file_t file, void* buf, size_t size, off_t index) {
  return -ENOSYS;
}

ssize_t bsfs_write(bs_file_t file, const void* buf, size_t size, off_t index) {
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
