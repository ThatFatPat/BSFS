#include "stego.h"
#include "bit_util.h"
#include "keytab.h"
#include "vector.h"
#include <errno.h>
#include <openssl/rand.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define KEYTAB_SIZE (KEYTAB_ENTRY_SIZE * MAX_LEVELS)

/**
 * Generating a random key.
 * The 2 last bytes are filled with 0.
 */
static int generate_random_key(uint8_t* buf) {
  if (RAND_bytes((unsigned char*) buf, STEGO_KEY_SIZE - 2) == 0) {
    return -1;
  }
  memset(buf + STEGO_KEY_SIZE - 2, 0, 2);
  return 0;
}

/**
 * Generating "count" orthonormal keys.
 * Uses special Gram-Schmidt to do so.
 */
int stego_gen_keys(void* buf, int count) {
  size_t total_keys_size = count * STEGO_KEY_SIZE;
  uint8_t* int_buf = (uint8_t*) buf;
  for (size_t i = 0; i < total_keys_size; i += STEGO_KEY_SIZE) {
    uint8_t* int_rnd_key = int_buf + i;
    if (generate_random_key(int_rnd_key) == -1) {
      return -1;
    }

    // Add to "rnd_key" all the keys before him which have scalar product 1 with
    // him.
    void* rnd_key = (void*) int_rnd_key;
    for (size_t j = 0; j < i; j += STEGO_KEY_SIZE) {
      int product = scalar_product(int_rnd_key, int_buf + j, STEGO_KEY_SIZE);
      vector_linear_combination(rnd_key, rnd_key, (void*) (int_buf + j),
                                STEGO_KEY_SIZE, product);
    }

    // If the norm of the key is 0, set the proper bit in the last two bytes.
    if (!norm(int_rnd_key, STEGO_KEY_SIZE)) {
      int key_num = i / STEGO_KEY_SIZE;
      if (key_num < 8) {
        int_rnd_key[STEGO_KEY_SIZE - 2] |= 1UL << key_num;
      } else {
        int_rnd_key[STEGO_KEY_SIZE - 1] |= 1UL << (key_num - 8);
      }
    }
  }
  return 0;
}

size_t compute_level_size(size_t disk_size) {
  if (disk_size < KEYTAB_SIZE) {
    return 0;
  }
  return (disk_size - KEYTAB_SIZE) / COVER_FILE_COUNT;
}

off_t cover_offset(size_t disk_size, int i) {
  return KEYTAB_SIZE + i * compute_level_size(disk_size);
}

/**
 * Calculate the linear combination of all the cover files by the key in a
 * specific range (between off and off + size).
 * The result is also the multiplication of the key vector with
 * the cover files matrix.
 */
void ranged_covers_linear_combination(const void* key, const void* disk_data,
                                      size_t disk_size, off_t off, void* buf,
                                      size_t read_size) {
  const_vector_t int_data = (uint8_t*) disk_data;

  memset(buf, 0, read_size);
  for (int i = 0; i < COVER_FILE_COUNT; i++) {
    bool bit = get_bit(key, i);
    off_t offset = cover_offset(disk_size, i) + off;
    vector_linear_combination((vector_t) buf, (const_vector_t) buf,
                              int_data + offset, read_size, bit);
  }
}

static int read_encrypted_level(const void* key, const void* disk,
                                size_t level_size, void* buf, off_t off,
                                size_t size) {
  return -ENOSYS;
}

static int write_level_encrypted(const void* key, bs_disk_t disk,
                                 const void* buf, off_t off, size_t size) {
  return -ENOSYS;
}

int stego_read_level(const void* key, bs_disk_t disk, void* buf, off_t off,
                     size_t size) {
  return -ENOSYS;
}

int stego_write_level(const void* key, bs_disk_t disk, const void* buf,
                      off_t off, size_t size) {
  return -ENOSYS;
}
