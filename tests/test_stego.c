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
  for (size_t disk_size = 0x2000; disk_size <= 0x200000; disk_size += 0x10000) {
    size_t level_size = stego_compute_user_level_size(disk_size);
    ck_assert_uint_le(KEYTAB_SIZE + STEGO_USER_LEVEL_COUNT * level_size,
                      disk_size);

    ck_assert_uint_eq(level_size % STEGO_LEVELS_PER_PASSWORD, 0);

    // Don't be tempted to just use `STEGO_USER_LEVEL_COUNT * (level_size + 1)`,
    // as there will most likely be wasted space to ensure that `level_size` is
    // a multiple of `STEGO_LEVELS_PER_PASSWORD`.
    ck_assert_uint_le(disk_size,
                      KEYTAB_SIZE +
                          STEGO_COVER_FILE_COUNT *
                              (level_size / STEGO_LEVELS_PER_PASSWORD + 1));
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
