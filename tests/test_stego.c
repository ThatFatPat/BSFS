#include "test_stego.h"
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stego.h>

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

static bool
check_orthonormality(void* buf,
                     int count) { // Don't check for linear independency
  uint8_t* int_buf = (uint8_t*) buf;
  size_t key_size = STEGO_KEY_BITS / CHAR_BIT;
  size_t total_keys_size = count * (key_size);
  for (size_t i = 0; i < total_keys_size; i += key_size) {
    uint8_t* key_1 = int_buf + i;
    if (norm(key_1, key_size) == 0) {
      return false;
    }
    for (size_t j = 0; j < i; j += key_size) {
      uint8_t* key_2 = int_buf + j;
      if (scalar_product(key_1, key_2, key_size) == 1) {
        return false;
      }
    }
  }
  return true;
}

START_TEST(test_gen_keys) {
  uint8_t buf[16 * (STEGO_KEY_BITS / CHAR_BIT)];
  for (int count = 12; count <= 16; count++) { // Check for 10 to 16 keys
    for (int i = 0; i < 15; i++) {
      stego_gen_keys(buf, count);
      ck_assert_msg(check_orthonormality(buf, count),
                    "Error in stego_gen_keys");
    }
  }
}
END_TEST

Suite* stego_suite(void) {
  Suite* suite = suite_create("stego");
  TCase* example_tcase = tcase_create("stego_gen_keys");
  tcase_add_test(example_tcase, test_gen_keys);
  suite_add_tcase(suite, example_tcase);

  return suite;
}
