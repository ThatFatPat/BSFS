#include "stego.h"
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

static bool scalar_product(const uint8_t* a, const uint8_t* b, size_t size) {
  bool ret = 0;
  for (size_t i = 0; i < size; i++) {
    ret ^= count_bits(a[i] & b[i]) & 1;
  }
  return ret;
}

static bool
norm(uint8_t* a,
     size_t size) { // Computing the norm of a, with "size" bytes length.
  return scalar_product(a, a, size);
}

static int generate_random_key(uint8_t* buf) { // Generating a random key. The 2
                                               // last bytes are filled with 0.
  int bytes_in_key = STEGO_KEY_BITS / CHAR_BIT;
  if (RAND_bytes((unsigned char*) buf, bytes_in_key - 2) == 0) {
    return -1;
  }
  memset(buf + bytes_in_key - 2, 0, 2);
  return 0;
}

int stego_gen_keys(void* buf,
                   int count) { // Generating "count" orthonormal keys. Uses
                                // special Gram-Schmidt to do so.
  size_t key_size = STEGO_KEY_BITS / CHAR_BIT;
  size_t total_keys_size = count * (key_size);
  uint8_t* int_buf = (uint8_t*) buf;
  for (size_t i = 0; i < total_keys_size; i += key_size) {
    uint8_t* rnd_key = int_buf + i;
    if (generate_random_key(rnd_key) == -1) {
      return -1;
    }
    for (size_t j = 0; j < i;
         j += key_size) { // Add to "rnd_key" all the keys before him which have
                          // scalar product 1 with him.
      int product =
          scalar_product(rnd_key, int_buf + j, STEGO_KEY_BITS / CHAR_BIT);
      if (product == 1) {
        for (size_t l = 0; l < key_size; l++) {
          rnd_key[l] ^= int_buf[j + l];
        }
      }
    }
    if (norm(rnd_key, key_size) == 0) { // If the norm of the key is 0, set the
                                        // proper bit in the last two bytes.
      int key_num = i / key_size;
      if (key_num < 8) {
        rnd_key[key_size - 2] |= 1UL << key_num;
      } else {
        rnd_key[key_size - 1] |= 1UL << (key_num - 8);
      }
    }
  }
  return 0;
}
