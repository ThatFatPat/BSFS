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
  uint8_t buf[MAX_LEVELS * STEGO_KEY_SIZE];
  for (size_t count = 1; count <= MAX_LEVELS; count++) {
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

START_TEST(test_compute_level_size) {
  for (size_t disk_size = 0x400; disk_size <= 0x2000000; disk_size += 0x800) {
    size_t level_size = compute_level_size(disk_size);
    ck_assert_uint_le(MAX_LEVELS * KEYTAB_ENTRY_SIZE +
                          COVER_FILE_COUNT * level_size,
                      disk_size);
  }
}
END_TEST

START_TEST(test_compute_level_size_too_small) {
  ck_assert_int_eq(compute_level_size(500), 0);
}
END_TEST

static bs_disk_t create_tmp_disk() {
  int fd = syscall(SYS_memfd_create, "test_stego.bsf", 0);
  ck_assert_int_ne(ftruncate(fd, 0x200000), -1); // 2MiB

  bs_disk_t disk;
  ck_assert_int_eq(disk_create(fd, &disk), 0);
  return disk;
}

START_TEST(test_read_write_level_roundtrip) {
  uint8_t keys[2 * STEGO_KEY_SIZE];
  ck_assert_int_eq(stego_gen_keys(keys, 2), 0);

  char data1[32] = "Hello!!! This is level 1!";
  char data2[32] = "And this is level 2!";

  char read1[sizeof(data1)] = { 0 };
  char read2[sizeof(data2)] = { 0 };

  bs_disk_t disk = create_tmp_disk();

  ck_assert_int_eq(stego_write_level(keys, disk, data1, 0, sizeof(data1)), 0);
  ck_assert_int_eq(
      stego_write_level(keys + STEGO_KEY_SIZE, disk, data2, 0, sizeof(data2)),
      0);

  ck_assert_int_eq(stego_read_level(keys, disk, read1, 0, sizeof(read1)), 0);
  ck_assert_int_eq(memcmp(read1, data1, sizeof(data1)), 0);

  ck_assert_int_eq(
      stego_read_level(keys + STEGO_KEY_SIZE, disk, read2, 0, sizeof(read2)),
      0);
  ck_assert_int_eq(memcmp(read2, data2, sizeof(data2)), 0);

  disk_free(disk);
}
END_TEST

Suite* stego_suite(void) {

  Suite* suite = suite_create("stego");

  TCase* stego_gen_keys_tcase = tcase_create("gen_keys");
  tcase_add_test(stego_gen_keys_tcase, test_gen_keys);
  tcase_add_test(stego_gen_keys_tcase, test_gen_keys_too_many);
  suite_add_tcase(suite, stego_gen_keys_tcase);

  TCase* level_size_tcase = tcase_create("level_size");
  tcase_add_test(level_size_tcase, test_compute_level_size);
  tcase_add_test(level_size_tcase, test_compute_level_size_too_small);
  suite_add_tcase(suite, level_size_tcase);

  TCase* stego_read_write_tcase = tcase_create("read_write");
  tcase_add_test(stego_read_write_tcase, test_read_write_level_roundtrip);
  suite_add_tcase(suite, stego_read_write_tcase);

  return suite;
}
