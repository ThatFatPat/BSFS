#include "bsfs_priv.h"

#include "stego.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#define FTAB_INITIAL_BUCKET_COUNT 8
#define FTAB_MAX_LOAD_FACTOR 1.f

static int realloc_buckets(bs_file_table_t* table, size_t bucket_count) {
  bs_file_t* new_buckets = (bs_file_t*) calloc(bucket_count, sizeof(bs_file_t));
  if (!new_buckets) {
    return -ENOMEM;
  }
  free(table->buckets);
  table->buckets = new_buckets;
  table->bucket_count = bucket_count;
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
