#include "stego.h"
#include <errno.h>
#include <limits.h>
#include <openssl/rand.h>
#include <stdint.h>

/**
 * Computing the scalar product of a and b, with "size" bytes length.
 */
static int scalar_product(uint8_t* a, uint8_t* b, int size) {
  int out = 0;
  for (int i = 0; i < size; i++) {
    for (int j = 0; j < CHAR_BIT; j++) {
      if (((a[i] >> j) & 1U) * ((b[i] >> j) & 1U) == 1) {
        out = !out;
      }
    }
  }
  return out;
}

/**
 * Computing the norm of a, with "size" bytes length.
 */
static int norm(uint8_t* a, int size) {
  int out = 0;
  for (int i = 0; i < size; i++) {
    for (int j = 0; j < CHAR_BIT; j++) {
      if (((a[i] >> j) & 1U) == 1) {
        out = !out;
      }
    }
  }
  return out;
}

/**
 * Generating a random key.
 * The 2 last bytes are filled with 0.
 */
static void generate_random_key(uint8_t* buf) {
  int bytes_in_key = STEGO_KEY_BITS / CHAR_BIT;
  buf = (uint8_t*) malloc(bytes_in_key);
  RAND_bytes((unsigned char*) buf, sizeof(buf) - 2);
  buf[bytes_in_key - 2] = 0;
  buf[bytes_in_key - 1] = 0;
}

/**
 * Generating "count" orthonormal keys.
 * Uses special Gram-Schmidt to do so.
 */
int stego_gen_keys(void* buf, int count) {
  int key_size = STEGO_KEY_BITS / CHAR_BIT;
  int total_keys_size = count * (key_size);
  uint8_t* keys = (uint8_t*) malloc(total_keys_size);
  for (int i = 0; i < total_keys_size; i += key_size) {
    uint8_t* rnd_key = NULL;
    generate_random_key(rnd_key);
    for (int j = 0; j < i;
         j += key_size) { // Add to "rnd_key" all the keys before him which have
                          // scalar product 1 with him.
      int product =
          scalar_product(rnd_key, &keys[j], STEGO_KEY_BITS / CHAR_BIT);
      if (product == 1) {
        for (int l = 0; l < key_size; l++) {
          rnd_key[l] += keys[j + l];
        }
      }
    }
    if (norm(rnd_key, key_size) == 1) { // If the norm of the key is 1, set the
                                        // proper bit in the last two bytes.
      int key_num = i / key_size;
      if (key_num < 8) {
        rnd_key[key_size - 2] |= 1UL << key_num;
      } else {
        rnd_key[key_size - 1] |= 1UL << (key_num - 8);
      }
    }
    for (int k = 0; k < key_size; k++) { // Insert the key into "keys".
      keys[i + k] = rnd_key[k];
    }
  }
  buf = keys;
  return 0;
}

static int read_encrypted_level(const void* key, const void* disk,
                                size_t level_size, void* buf, off_t off,
                                size_t size) {
  return ENOSYS;
}

static int write_level_encrypted(const void* key, bs_disk_t disk,
                                 const void* buf, off_t off, size_t size) {
  return ENOSYS;
}

static int calculate_cover_files_sum(bs_disk_t disk, off_t first_cover,
                                     off_t second_cover) {
  return ENOSYS;
}

static int calculate_files_diffrence(void* minuend, void* subtrahend,
                                     size_t files_size) {
  return ENOSYS;
}

int stego_read_level(const void* key, bs_disk_t disk, void* buf, off_t off,
                     size_t size) {
  return ENOSYS;
}

int stego_write_level(const void* key, bs_disk_t disk, const void* buf,
                      off_t off, size_t size) {
  return ENOSYS;
}
