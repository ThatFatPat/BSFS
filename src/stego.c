#include "stego.h"
#include "keytab.h"
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <openssl/rand.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define KEYTAB_SIZE (KEYTAB_ENTRY_SIZE * MAX_LEVELS)

static int vector_linear_combination(void* linear_combination,
                                     void* first_vector, void* second_vector,
                                     size_t vector_size, bool coefficient);

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
    uint8_t* int_rnd_key = int_buf + i;
    if (generate_random_key(int_rnd_key) == -1) {
      return -1;
    }

    // Add to "rnd_key" all the keys before him which have scalar product 1 with
    // him.
    void* rnd_key = (void*) int_rnd_key;
    for (size_t j = 0; j < i; j += STEGO_KEY_BYTES) {
      int product = scalar_product(int_rnd_key, int_buf + j, STEGO_KEY_BYTES);
      vector_linear_combination(rnd_key, rnd_key, (void*) (int_buf + j),
                                STEGO_KEY_BYTES, product);
    }

    // If the norm of the key is 0, set the proper bit in the last two bytes.
    if (!norm(int_rnd_key, STEGO_KEY_BYTES)) {
      int key_num = i / STEGO_KEY_BYTES;
      if (key_num < 8) {
        int_rnd_key[STEGO_KEY_BYTES - 2] |= 1UL << key_num;
      } else {
        int_rnd_key[STEGO_KEY_BYTES - 1] |= 1UL << (key_num - 8);
      }
    }
  }
  return 0;
}

size_t compute_level_size(size_t disk_size) {
  return floor((float) (disk_size - KEYTAB_SIZE) / STEGO_KEY_BITS);
}

static off_t cover_offset(bs_disk_t disk, int i) {
  return KEYTAB_SIZE + i * compute_level_size(disk_get_size(disk));
}

/**
 * Calculate the first_vector + coefficient * second_vector.
 * Put the result into linear_combination.
 */
static int vector_linear_combination(void* linear_combination,
                                     void* first_vector, void* second_vector,
                                     size_t vector_size, bool coefficient) {

  uint8_t* int_linear_combination = (uint8_t*) linear_combination;
  uint8_t* int_first_vector = (uint8_t*) first_vector;
  uint8_t* int_second_vector = (uint8_t*) second_vector;

  for (size_t i = 0; i < vector_size; i++) {
    int_linear_combination[i] = int_first_vector[i];
    if (coefficient) {
      int_linear_combination[i] ^= int_second_vector[i];
    }
  }

  return 0;
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
