#ifndef BS_STEGO_H
#define BS_STEGO_H

#include "disk.h"
#include "enc.h"
#include <stdlib.h>

#define STEGO_KEY_BITS 128

int stego_read_level(const void* key, bs_disk_t disk, void* buf, off_t off,
                     size_t size);

int stego_write_level(const void* key, bs_disk_t disk, const void* buf,
                      off_t off, size_t size);

int stego_gen_keys(void* buf, int count);

#endif // BS_STEGO_H
