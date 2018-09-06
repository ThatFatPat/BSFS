#include "bft.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

int bft_entry_init(bft_entry_t* ent, const char* name, size_t size, mode_t mode,
  uint32_t initial_cluster, bft_timestamp_t atim, bft_timestamp_t mtim) {
  ent->name = strdup(name);
  if (!ent->name) {
    return -errno;
  }

  ent->size = size;
  ent->mode = mode;
  ent->initial_cluster = initial_cluster;
  ent->atim = atim;
  ent->mtim = mtim;

  return 0;
}

void bft_entry_destroy(bft_entry_t* ent) {
  // Const-removal is safe if `ent` was initialized with `bft_entry_init`.
  free((void*) ent->name);
  *ent = (bft_entry_t) {0}; // turn use-after-free into a guaranteed crash
}


int bft_find_free_table_entry(const void* bft, bft_offset_t* off) {
  const uint8_t* bft_bytes = (const uint8_t*) bft;

  for (bft_offset_t i = 0; i < BFT_MAX_ENTRIES; i++) {
    if (bft_bytes[i * BFT_ENTRY_SIZE] == 0) {
      *off = i;
      return 0;
    }
  }
  return -ENOSPC;
}
