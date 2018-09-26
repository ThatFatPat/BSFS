#include "stego.h"
#include <errno.h>
#include <limits.h>
#include <openssl/rand.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int count_bits(uint8_t a) {
  int ret = 0;
  while (a) {
    ret++;
    a &= a - 1; // move to next bit
  }
  return ret;
}

/**
 * Computing the scalar product of a and b, with "size" bytes length.
 */
static bool scalar_product(const uint8_t* a, const uint8_t* b, size_t size) {
  bool ret = 0;
  for (size_t i = 0; i < size; i++) {
    ret ^= count_bits(a[i] & b[i]) & 1;
  }
  return ret;
}

/**
 * Computing the norm of a, with "size" bytes length.
 */
static bool norm(uint8_t* a, size_t size) {
  return scalar_product(a, a, size);
}

/**
 * Generating a random key.
 * The 2 last bytes are filled with 0.
 */
static int generate_random_key(uint8_t* buf) {
  if (RAND_bytes((unsigned char*) buf, STEGO_KEY_BYTES - 2) == 0) {
    return -1;
  }
  memset(buf + STEGO_KEY_BYTES - 2, 0, 2);
  return 0;
}

/**
 * Generating "count" orthonormal keys.
 * Uses special Gram-Schmidt to do so.
 */
int stego_gen_keys(void* buf, int count) {
  size_t total_keys_size = count * (STEGO_KEY_BYTES);
  uint8_t* int_buf = (uint8_t*) buf;
  for (size_t i = 0; i < total_keys_size; i += STEGO_KEY_BYTES) {
    uint8_t* rnd_key = int_buf + i;
    if (generate_random_key(rnd_key) == -1) {
      return -1;
    }
    for (size_t j = 0; j < i;
         j += STEGO_KEY_BYTES) { // Add to "rnd_key" all the keys before him
                                 // which have
                                 // scalar product 1 with him.
      int product = scalar_product(rnd_key, int_buf + j, STEGO_KEY_BYTES);
      if (product) {
        for (size_t l = 0; l < STEGO_KEY_BYTES; l++) {
          rnd_key[l] ^= int_buf[j + l];
        }
      }
    }

    if (!norm(rnd_key,
              STEGO_KEY_BYTES)) { // If the norm of the key is 0, set the
                                  // proper bit in the last two bytes.
      int key_num = i / STEGO_KEY_BYTES;
      if (key_num < 8) {
        rnd_key[STEGO_KEY_BYTES - 2] |= 1UL << key_num;
      } else {
        rnd_key[STEGO_KEY_BYTES - 1] |= 1UL << (key_num - 8);
      }
    }
  }
  return 0;
}

static off_t cover_offset(int i) {
  return -ENOSYS;
}

/**
 * Calculate the linear combination of
 */
static int vector_linear_combination(void* linear_combination,
                                     uint8_t* coefficients,
                                     size_t coefficients_size, void* vectors,
                                     size_t vector_size, size_t vectors_size) {
  return -ENOSYS;
}

/**
 * Calculate the linear combination of all the cover files by the key in a
 * specific range (between off and off + size).
 * The result is also the multiplication of the key vector with
 * the cover files matrix.
 */
static int ranged_covers_linear_combination(const void* key, bs_disk_t disk,
                                            off_t off, size_t size, void* buf) {
  return -ENOSYS;
}

static int direct_read(bs_disk_t disk, off_t off, void* buf, size_t size) {
  return -ENOSYS;
}

static int direct_write(bs_disk_t disk, off_t off, void* buf, size_t size) {
  return -ENOSYS;
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
