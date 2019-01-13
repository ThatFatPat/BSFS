#include "bsfs.h"

#include "bft.h"
#include "disk.h"
#include "stego.h"
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
