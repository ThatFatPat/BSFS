#ifndef BS_BFT_H
#define BS_BFT_H

#include "cluster.h"
#include "disk.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#define BFT_MAX_FILENAME 64
#define BFT_ENTRY_SIZE 84 // 64B + 5 * 4B
#define BFT_MAX_ENTRIES 8192
#define BFT_SIZE (BFT_ENTRY_SIZE * BFT_MAX_ENTRIES)

typedef uint32_t bft_timestamp_t;
typedef int16_t bft_offset_t;

typedef struct bft_entry {
  const char* name;
  cluster_offset_t initial_cluster;
  size_t size;
  mode_t mode;
  bft_timestamp_t atim;
  bft_timestamp_t mtim;
} bft_entry_t;

typedef bool (*bft_entry_iter_t)(bft_offset_t, const bft_entry_t*, void*);

int bft_entry_init(bft_entry_t* ent, const char* name, size_t size, mode_t mode,
                   cluster_offset_t initial_cluster, bft_timestamp_t atim,
                   bft_timestamp_t mtim);
void bft_entry_destroy(bft_entry_t* ent);

int bft_find_free_table_entry(const void* bft, bft_offset_t* off);

int bft_read_table_entry(const void* bft, bft_entry_t* ent, bft_offset_t off);
int bft_write_table_entry(void* bft, const bft_entry_t* ent, bft_offset_t off);
int bft_remove_table_entry(void* bft, bft_offset_t off);

int bft_iter_table_entries(const void* bft, bft_entry_iter_t iter, void* ctx);
int bft_find_table_entry(const void* bft, const char* filename,
                         bft_offset_t* off);

int bft_read_table(const void* key, bs_disk_t disk, void* bft);
int bft_write_table(const void* key, bs_disk_t disk, void* bft);

#endif // BS_BFT_H
