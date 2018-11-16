#include "bft.h"

#include "bit_util.h"
#include "stego.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>

int bft_entry_init(bft_entry_t* ent, const char* name, size_t size, mode_t mode,
                   cluster_offset_t initial_cluster, bft_timestamp_t atim,
                   bft_timestamp_t mtim) {
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
  *ent = (bft_entry_t){ 0 }; // turn use-after-free into a guaranteed crash
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

static int do_read_entry(const uint8_t* raw_ent, bft_entry_t* ent) {
  if (raw_ent[BFT_MAX_FILENAME - 1] != 0) {
    return -EIO; // missing null terminator on filename
  }

  ent->name = (const char*) raw_ent;
  raw_ent += BFT_MAX_FILENAME;

  ent->initial_cluster = read_big_endian(raw_ent);
  raw_ent += sizeof(uint32_t);

  ent->size = read_big_endian(raw_ent);
  raw_ent += sizeof(uint32_t);

  ent->mode = read_big_endian(raw_ent);
  raw_ent += sizeof(uint32_t);

  ent->atim = read_big_endian(raw_ent);
  raw_ent += sizeof(uint32_t);

  ent->mtim = read_big_endian(raw_ent);
  return 0;
}

static int do_write_entry(uint8_t* raw_ent, const bft_entry_t* ent) {
  if (strlen(ent->name) >= BFT_MAX_FILENAME) {
    return -ENAMETOOLONG;
  }

  strncpy((char*) raw_ent, ent->name, BFT_MAX_FILENAME);
  raw_ent += BFT_MAX_FILENAME;

  write_big_endian(raw_ent, ent->initial_cluster);
  raw_ent += sizeof(uint32_t);

  write_big_endian(raw_ent, ent->size);
  raw_ent += sizeof(uint32_t);

  write_big_endian(raw_ent, ent->mode);
  raw_ent += sizeof(uint32_t);

  write_big_endian(raw_ent, ent->atim);
  raw_ent += sizeof(uint32_t);

  write_big_endian(raw_ent, ent->mtim);
  return 0;
}

int bft_read_table_entry(const void* bft, bft_entry_t* ent, bft_offset_t off) {
  if (off >= BFT_MAX_ENTRIES) {
    return -EINVAL;
  }

  bft_entry_t direct_ent;
  int read_status =
      do_read_entry((const uint8_t*) bft + off * BFT_ENTRY_SIZE, &direct_ent);
  if (read_status < 0) {
    return read_status;
  }
  return bft_entry_init(ent, direct_ent.name, direct_ent.size, direct_ent.mode,
                        direct_ent.initial_cluster, direct_ent.atim,
                        direct_ent.mtim);
}

int bft_write_table_entry(void* bft, const bft_entry_t* ent, bft_offset_t off) {
  if (off >= BFT_MAX_ENTRIES) {
    return -EINVAL;
  }
  return do_write_entry((uint8_t*) bft + off * BFT_ENTRY_SIZE, ent);
}

int bft_remove_table_entry(void* bft, bft_offset_t off) {
  if (off >= BFT_MAX_ENTRIES) {
    return -EINVAL;
  }
  memset((uint8_t*) bft + off * BFT_ENTRY_SIZE, 0, BFT_ENTRY_SIZE);
  return 0;
}

int bft_iter_table_entries(const void* bft, bft_entry_iter_t iter, void* ctx) {
  const uint8_t* bft_bytes = (const uint8_t*) bft;

  for (bft_offset_t off = 0; off < BFT_MAX_ENTRIES; off++) {
    const uint8_t* raw_ent = bft_bytes + off * BFT_ENTRY_SIZE;
    if (*raw_ent) { // non-empty entry
      bft_entry_t ent;
      int status = do_read_entry(raw_ent, &ent);
      if (status < 0) {
        return status;
      }
      if (!iter(off, &ent, ctx)) {
        break;
      }
    }
  }

  return 0;
}

struct find_entry_ctx {
  const char* name;
  bft_offset_t* off;
  bool found;
};

static bool find_entry_iter(bft_offset_t off, const bft_entry_t* ent,
                            void* raw_ctx) {
  struct find_entry_ctx* ctx = (struct find_entry_ctx*) raw_ctx;

  if (strcmp(ent->name, ctx->name) == 0) {
    *ctx->off = off;
    ctx->found = true;
    return false;
  }

  return true;
}

int bft_find_table_entry(const void* bft, const char* filename,
                         bft_offset_t* off) {
  struct find_entry_ctx ctx = { .name = filename, .off = off, .found = false };

  int iter_status = bft_iter_table_entries(bft, find_entry_iter, &ctx);
  if (iter_status < 0) {
    return iter_status;
  }
  if (!ctx.found) {
    return -ENOENT;
  }

  return 0;
}

int bft_read_table(const void* key, bs_disk_t disk, void* bft) {
  return stego_read_level(key, disk, bft, 0, BFT_SIZE);
}

int bft_write_table(const void* key, bs_disk_t disk, void* bft) {
  return stego_write_level(key, disk, bft, 0, BFT_SIZE);
}
