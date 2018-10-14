#include "test_stego.h"
#include "bit_util.h"
#include "disk.h"
#include "keytab.h"
#include "stego.h"
#include "vector.h"
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <unistd.h>

/**
 * Don't check for linear independency
 */
static bool check_orthonormality(void* buf, int count) {
  uint8_t* int_buf = (uint8_t*) buf;
  size_t key_size = COVER_FILE_COUNT / CHAR_BIT;
  size_t total_keys_size = count * (key_size);
  for (size_t i = 0; i < total_keys_size; i += key_size) {
    uint8_t* key_1 = int_buf + i;
    if (vector_norm(key_1, key_size) == 0) {
      return false;
    }
    for (size_t j = 0; j < i; j += key_size) {
      uint8_t* key_2 = int_buf + j;
      if (vector_scalar_product(key_1, key_2, key_size) == 1) {
        return false;
      }
    }
  }
  return true;
}

START_TEST(test_gen_keys) {
  uint8_t buf[16 * (COVER_FILE_COUNT / CHAR_BIT)];
  for (int count = 12; count <= 16; count++) { // Check for 10 to 16 keys
    for (int i = 0; i < 15; i++) {
      stego_gen_keys(buf, count);
      ck_assert_msg(check_orthonormality(buf, count),
                    "Error in stego_gen_keys");
    }
  }
}
END_TEST

/*-----------------------------------------------------------------------------------*/

#define TEST_STEGO_DISK_SIZE 512 + COVER_FILE_COUNT* STEGO_KEY_SIZE

static int open_tmp_file() {
  int fptr = syscall(SYS_memfd_create, "data.bsf", 0);
  char content[] = "Hello, I'm the Doctor.\n Basically, Run.\n";
  write(fptr, content, sizeof(content));
  return fptr;
}

START_TEST(test_linear_combination) {
  int vector1 = 38262900;
  int vector2 = 93374014;
  int linear_combination1;
  int linear_combination2;
  vector_linear_combination((void*) &linear_combination1, (void*) &vector1,
                            (void*) &vector2, sizeof(int), 0);
  vector_linear_combination((void*) &linear_combination2, (void*) &vector1,
                            (void*) &vector2, sizeof(int), 1);
  ck_assert_uint_eq(linear_combination1, vector1);
  ck_assert_uint_eq(linear_combination2, vector1 ^ vector2);
}
END_TEST

START_TEST(test_cover_linear_combination) {
  uint8_t writable_disk[TEST_STEGO_DISK_SIZE] = { 0 };
  for (int i = 0; i < CHAR_BIT; i++) {
    set_bit(writable_disk + cover_offset(TEST_STEGO_DISK_SIZE, i), i, 1);
  }

  uint8_t int_buf;
  void* buf = (void*) &int_buf;
  uint8_t key1[STEGO_KEY_SIZE];
  key1[0] = -1;
  uint8_t key2[STEGO_KEY_SIZE];
  key2[0] = -2;
  uint8_t key3[STEGO_KEY_SIZE];
  key3[0] = 1;
  uint8_t key4[STEGO_KEY_SIZE];
  key4[0] = 0;
  ranged_covers_linear_combination(key1, writable_disk, TEST_STEGO_DISK_SIZE, 0,
                                   buf, CHAR_BIT);
  ck_assert_int_eq(int_buf, key1[0]);

  ranged_covers_linear_combination(key2, writable_disk, TEST_STEGO_DISK_SIZE, 0,
                                   buf, CHAR_BIT);
  ck_assert_int_eq(int_buf, key2[0]);

  ranged_covers_linear_combination(key3, writable_disk, TEST_STEGO_DISK_SIZE, 0,
                                   buf, CHAR_BIT);
  ck_assert_int_eq(int_buf, key3[0]);

  ranged_covers_linear_combination(key4, writable_disk, TEST_STEGO_DISK_SIZE, 0,
                                   buf, CHAR_BIT);
  ck_assert_int_eq(int_buf, key4[0]);
}
END_TEST

/*-----------------------------------------------------------------------------------*/

Suite* stego_suite(void) {

  Suite* suite = suite_create("stego");
  TCase* stego_gen_keys_tcase = tcase_create("stego_gen_keys");
  TCase* stego_read_write_tcase = tcase_create("stego_read_write");

  tcase_add_test(stego_gen_keys_tcase, test_gen_keys);
  suite_add_tcase(suite, stego_gen_keys_tcase);

  tcase_add_test(stego_read_write_tcase, test_linear_combination);
  tcase_add_test(stego_read_write_tcase, test_cover_linear_combination);
  suite_add_tcase(suite, stego_read_write_tcase);

  return suite;
}
