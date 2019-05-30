#ifndef BS_STEGO_H
#define BS_STEGO_H

#include "disk.h"
#include "enc.h"
#include <limits.h>
#include <stdint.h>
#include <sys/types.h>

#define STEGO_COVER_FILE_COUNT 256
#define STEGO_KEY_SIZE (STEGO_COVER_FILE_COUNT / CHAR_BIT)
#define STEGO_LEVELS_PER_PASSWORD 4
#define STEGO_USER_LEVEL_COUNT 16

typedef struct {
  uint8_t aes_key[ENC_KEY_SIZE];
  uint8_t read_keys[STEGO_LEVELS_PER_PASSWORD][STEGO_KEY_SIZE];
  uint8_t write_keys[STEGO_LEVELS_PER_PASSWORD][STEGO_KEY_SIZE];
} stego_key_t;

size_t stego_compute_user_level_size(size_t disk_size);

int stego_gen_user_keys(stego_key_t* keys, size_t count);

int stego_read_level(const stego_key_t* key, bs_disk_t disk, void* buf,
                     off_t off, size_t size);
int stego_write_level(const stego_key_t* key, bs_disk_t disk, const void* buf,
                      off_t off, size_t size);

#endif // BS_STEGO_H
