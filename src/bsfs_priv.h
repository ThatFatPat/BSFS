#ifndef BS_BSFS_PRIV_H
#define BS_BSFS_PRIV_H

#include "bsfs.h"

#include "bft.h"
#include "disk.h"
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>

struct bs_file_impl {
  struct bs_open_level_impl* level;

  bft_offset_t index;
  atomic_int refcount;
  pthread_rwlock_t lock; // Protects reads and writes to the file

  bs_file_t next; // For chaining in the table
};

typedef struct bs_oft {
  bs_file_t* buckets;
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
int bs_oft_has(bs_oft_t* table, bft_offset_t index, bool* out);
int bs_oft_release(bs_oft_t* table, bs_file_t file);

int bs_level_get(bs_bsfs_t fs, const char* pass, bs_open_level_t* out);

int bs_get_dirname(const char* path, char** out_pass);
int bs_split_path(const char* path, char** out_pass, char** out_name);

/**
 * Append the contents of `buf` at offset `off` past the end of the file (as
 * specified by `cluster_idx` and `local_eof_off`).
 */
int bs_do_write_extend(bs_open_level_t level, cluster_offset_t cluster_idx,
                       off_t local_eof_off, const void* buf, size_t buf_size,
                       off_t off);

#endif
