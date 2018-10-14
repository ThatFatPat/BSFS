#include "test_stego.h"

#include "bit_util.h"
#include "disk.h"
#include "keytab.h"
#include "stego.h"
#include "vector.h"
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <unistd.h>

/**
 * Don't check for linear independency
 */
static bool check_orthonormality(void* buf, size_t count) {
  uint8_t* int_buf = (uint8_t*) buf;
  for (size_t i = 0; i < count; i++) {
    uint8_t* key_1 = int_buf + i * STEGO_KEY_SIZE;

    if (vector_norm(key_1, STEGO_KEY_SIZE) == 0) {
      return false;
    }

    for (size_t j = 0; j < i; j++) {
      uint8_t* key_2 = int_buf + j * STEGO_KEY_SIZE;
      if (vector_scalar_product(key_1, key_2, STEGO_KEY_SIZE) == 1) {
        return false;
      }
    }
  }

  return true;
}

START_TEST(test_gen_keys) {
  uint8_t buf[16 * STEGO_KEY_SIZE];
  for (size_t count = 1; count <= 16; count++) {
    ck_assert_int_eq(stego_gen_keys(buf, count), 0);
    ck_assert(check_orthonormality(buf, count));
  }
}
END_TEST

START_TEST(test_gen_keys_too_many) {
  uint8_t buf[(MAX_LEVELS + 1) * STEGO_KEY_SIZE];
  ck_assert_int_eq(stego_gen_keys(buf, MAX_LEVELS + 1), -EINVAL);
}
END_TEST

/*-----------------------------------------------------------------------------------*/

#define TEST_STEGO_DISK_SIZE (512 + COVER_FILE_COUNT * STEGO_KEY_SIZE)

START_TEST(test_cover_linear_combination) {
  uint8_t writable_disk[TEST_STEGO_DISK_SIZE] = { 0 };
  for (int i = 0; i < CHAR_BIT; i++) {
    set_bit(writable_disk + cover_offset(TEST_STEGO_DISK_SIZE, i), i, 1);
  }

  uint8_t int_buf;
  void* buf = (void*) &int_buf;
  uint8_t key1[STEGO_KEY_SIZE] = { -1 };
  uint8_t key2[STEGO_KEY_SIZE] = { -2 };
  uint8_t key3[STEGO_KEY_SIZE] = { 1 };
  uint8_t key4[STEGO_KEY_SIZE] = { 0 };
  ranged_covers_linear_combination(key1, writable_disk, TEST_STEGO_DISK_SIZE, 0,
                                   buf, 1);
  ck_assert_int_eq(int_buf, key1[0]);

  ranged_covers_linear_combination(key2, writable_disk, TEST_STEGO_DISK_SIZE, 0,
                                   buf, 1);
  ck_assert_int_eq(int_buf, key2[0]);

  ranged_covers_linear_combination(key3, writable_disk, TEST_STEGO_DISK_SIZE, 0,
                                   buf, 1);
  ck_assert_int_eq(int_buf, key3[0]);

  ranged_covers_linear_combination(key4, writable_disk, TEST_STEGO_DISK_SIZE, 0,
                                   buf, 1);
  ck_assert_int_eq(int_buf, key4[0]);
}
END_TEST

/*-----------------------------------------------------------------------------------*/

Suite* stego_suite(void) {

  Suite* suite = suite_create("stego");

  TCase* stego_gen_keys_tcase = tcase_create("gen_keys");
  tcase_add_test(stego_gen_keys_tcase, test_gen_keys);
  tcase_add_test(stego_gen_keys_tcase, test_gen_keys_too_many);
  suite_add_tcase(suite, stego_gen_keys_tcase);

  TCase* stego_read_write_tcase = tcase_create("read_write");
  tcase_add_test(stego_read_write_tcase, test_cover_linear_combination);
  suite_add_tcase(suite, stego_read_write_tcase);

  return suite;
}
