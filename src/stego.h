#ifndef BS_STEGO_H
#define BS_STEGO_H

#include <stdlib.h>

#define STEGO_KEY_SIZE 128

int stego_read_level(const void* key, const void* disk, size_t level_size,
  void* buf, off_t off, size_t size);

int stego_write_level(const void* key, void* disk, size_t level_size,
  const void* buf, off_t off, size_t size);

int stego_gen_keys(void* buf, int count);


#endif // BS_STEGO_H
