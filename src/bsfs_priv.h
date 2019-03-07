#ifndef BS_BSFS_PRIV_H
#define BS_BSFS_PRIV_H

#include "bsfs.h"

#include "bft.h"
#include "disk.h"
#include <pthread.h>
#include <stdatomic.h>

struct bs_file_impl {
  struct bs_open_level_impl* level;

  bft_offset_t index;
  atomic_int refcount;
  pthread_rwlock_t lock; // Protects reads and writes to the file

  bs_file_t next; // For chaining in the table
};

typedef struct bs_oft {
  bs_file_t head;
  bs_file_t** buckets; // Array of next pointers
  size_t bucket_count;
  size_t size;
  pthread_mutex_t lock; // Protects all table operations
} bs_oft_t;

typedef struct bs_open_level_impl {
  bs_bsfs_t fs;
  stego_key_t key;
  char* pass;
  void* bft;
  void* bitmap;
  bs_oft_t open_files;
  pthread_rwlock_t metadata_lock; // Protects BFT and bitmap
} * bs_open_level_t;

struct bs_bsfs_impl {
  struct bs_open_level_impl levels[STEGO_USER_LEVEL_COUNT];
  pthread_mutex_t level_lock; // Protects `levels`
  bs_disk_t disk;
};

int bs_oft_init(bs_oft_t* table);
void bs_oft_destroy(bs_oft_t* table);

int bs_oft_get(bs_oft_t* table, bs_open_level_t level, bft_offset_t index,
               bs_file_t* file);
int bs_oft_release(bs_oft_t* table, bs_file_t file);

int bs_level_get(bs_bsfs_t fs, const char* pass, bs_open_level_t* out);

int bs_split_path(const char* path, char** out_pass, char** out_name);

#endif
