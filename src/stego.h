#ifndef BS_STEGO_H
#define BS_STEGO_H

#include "disk.h"
#include "enc.h"
#include <sys/types.h>

#define COVER_FILE_COUNT 128
#define STEGO_KEY_SIZE (COVER_FILE_COUNT / CHAR_BIT)

int stego_gen_keys(void* buf, int count);

size_t compute_level_size(size_t disk_size);

off_t cover_offset(bs_disk_t disk, int i);

int ranged_covers_linear_combination(const void* key, bs_disk_t disk, off_t off,
                                     size_t size, void* buf);

int stego_read_level(const void* key, bs_disk_t disk, void* buf, off_t off,
                     size_t size);

int stego_write_level(const void* key, bs_disk_t disk, const void* buf,
                      off_t off, size_t size);

#endif // BS_STEGO_H
