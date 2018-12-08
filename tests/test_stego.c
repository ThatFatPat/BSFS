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

START_TEST(test_compute_level_size) {
  for (size_t disk_size = 0x400; disk_size <= 0x200000; disk_size += 0x8000) {
    size_t level_size = stego_compute_user_level_size(disk_size);
    ck_assert_uint_le(KEYTAB_SIZE + STEGO_COVER_FILE_COUNT * level_size,
                      disk_size);
    ck_assert_uint_le(disk_size,
                      KEYTAB_SIZE + STEGO_COVER_FILE_COUNT * (level_size + 1));
  }
}
END_TEST

START_TEST(test_compute_level_size_too_small) {
  ck_assert_int_eq(stego_compute_user_level_size(500), 0);
}
END_TEST

Suite* stego_suite(void) {
  Suite* suite = suite_create("stego");

  TCase* level_size_tcase = tcase_create("level_size");
  tcase_add_test(level_size_tcase, test_compute_level_size);
  tcase_add_test(level_size_tcase, test_compute_level_size_too_small);
  suite_add_tcase(suite, level_size_tcase);

  return suite;
}
