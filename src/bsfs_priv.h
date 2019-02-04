#ifndef BS_BSFS_PRIV_H
#define BS_BSFS_PRIV_H

#include "bsfs.h"
#include "disk.h"
#include <pthread.h>
#include <stdatomic.h>

struct bs_file_impl {
  struct bs_open_level_impl* level;
  bft_offset_t index;
  atomic_int refcount; // Atomically counts the amount of references to file.
  pthread_rwlock_t file_lock;
  bs_file_t next;
};

struct bs_file_table {
  bs_file_t head;
  bs_file_t* buckets;
  size_t bucket_count;
  size_t size;
};

struct bs_open_level_impl {
  bs_bsfs_t bsfs;
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

#endif
