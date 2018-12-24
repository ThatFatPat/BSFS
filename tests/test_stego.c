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

static void validate_keys(const stego_key_t* keys, size_t count) {
  for (size_t i1 = 0; i1 < count; i1++) {
    const stego_key_t* key = keys + i1;

    for (size_t j1 = 0; j1 < STEGO_LEVELS_PER_PASSWORD; j1++) {
      const uint8_t* read_key = key->read_keys[j1];

      for (size_t i2 = 0; i2 < count; i2++) {
        const stego_key_t* other_key = keys + i2;

        for (size_t j2 = 0; j2 < STEGO_LEVELS_PER_PASSWORD; j2++) {
          const uint8_t* write_key = other_key->write_keys[j2];

          // A read key and a write key should have an inner product of 1 iff
          // they correspond to one another.
          ck_assert_int_eq(
              vector_scalar_product(read_key, write_key, STEGO_KEY_SIZE),
              i1 == i2 && j1 == j2);
        }
      }
    }
  }
}

START_TEST(test_gen_keys) {
  stego_key_t keys[STEGO_USER_LEVEL_COUNT];
  for (size_t count = 0; count <= STEGO_USER_LEVEL_COUNT; count++) {
    ck_assert_int_eq(stego_gen_user_keys(keys, count), 0);
    validate_keys(keys, count);
  }
}
END_TEST

START_TEST(test_gen_keys_too_many) {
  stego_key_t keys[STEGO_USER_LEVEL_COUNT + 1];
  ck_assert_int_eq(stego_gen_user_keys(keys, STEGO_USER_LEVEL_COUNT + 1),
                   -EINVAL);
}
END_TEST

START_TEST(test_compute_level_size) {
  for (size_t disk_size = 0x2000; disk_size <= 0x200000; disk_size += 0x8000) {
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

  TCase* gen_keys_tcase = tcase_create("gen_keys");
  tcase_add_test(gen_keys_tcase, test_gen_keys);
  tcase_add_test(gen_keys_tcase, test_gen_keys_too_many);
  suite_add_tcase(suite, gen_keys_tcase);

  TCase* level_size_tcase = tcase_create("level_size");
  tcase_add_test(level_size_tcase, test_compute_level_size);
  tcase_add_test(level_size_tcase, test_compute_level_size_too_small);
  suite_add_tcase(suite, level_size_tcase);

  return suite;
}
