#include "bsfs_priv.h"

#include "stego.h"
#include <errno.h>
#include <pthread.h>

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
