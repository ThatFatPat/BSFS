#ifndef BS_BSFS_PRIV_H
#define BS_BSFS_PRIV_H

#include "bsfs.h"
#include "disk.h"
#include <pthread.h>
#include <stdatomic.h>

struct bs_file_impl {
  struct bs_open_level_impl* level;

  bft_offset_t index;
  atomic_int refcount;
  pthread_rwlock_t file_lock; // Protects reads and writes to the file

  bs_file_t next; // For chaining in the table
};

struct bs_file_table {
  bs_file_t head;
  bs_file_t* buckets;
  size_t bucket_count;
  size_t size;
  pthread_mutex_t lock; // Protects all table operations
};

struct bs_open_level_impl {
  bs_bsfs_t fs;
  stego_key_t key;
  char* pass;
  void* bft;
  void* bitmap;
  struct bs_file_table open_files;
  pthread_rwlock_t metadata_lock; // Protects BFT and bitmap
};

struct bs_bsfs_impl {
  struct bs_open_level_impl levels[STEGO_USER_LEVEL_COUNT];
  pthread_mutex_t level_lock; // Protects `levels`
  bs_disk_t disk;
};

int bs_file_table_init(struct bs_file_table* table);
void bs_file_table_destroy(struct bs_file_table* table);

int bs_file_table_open(struct bs_file_table* table, bft_offset_t off,
                       bs_file_t* file);
int bs_file_table_release(struct bs_file_table* table, bs_file_t file);

#endif
