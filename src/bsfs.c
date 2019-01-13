#include "bsfs.h"

#include "bft.h"
#include "disk.h"
#include "stego.h"
#include <errno.h>
#include <pthread.h>

struct bs_file_impl {
  struct bs_open_level_impl* level;
  bft_offset_t index;
  _Atomic int refcount; // Atomically counts the amount of references to file.
  pthread_rwlock_t file_lock;
};

struct bs_open_level_impl {
  struct bs_bsfs_impl* bsfs;
  stego_key_t key;
  char* pass;
  void* bft;
  void* bitmap;
  // TODO: open_files
  pthread_mutex_t ftab_lock;      // Locks the Open File Table.
  pthread_rwlock_t metadata_lock; // Used for locking BFT and Bitmap.
};

struct bs_bsfs_impl {
  struct bs_open_level_impl levels[STEGO_USER_LEVEL_COUNT];
  bs_disk_t disk;
  pthread_mutex_t level_lock;
};

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