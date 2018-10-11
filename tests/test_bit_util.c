#include "test_bit_util.h"

#include "bit_util.h"
#include <string.h>

START_TEST(test_write_big_endian) {
  uint32_t val = 0xdeadbeef;
  uint8_t expected[] = { 0xde, 0xad, 0xbe, 0xef };

  uint8_t buf[sizeof(uint32_t)];
  write_big_endian(buf, val);
  ck_assert_int_eq(memcmp(buf, expected, sizeof(uint32_t)), 0);
}
END_TEST

START_TEST(test_read_big_endian) {
  uint8_t buf[] = { 0xde, 0xad, 0xbe, 0xef };
  ck_assert_uint_eq(read_big_endian(buf), 0xdeadbeef);
}
END_TEST

Suite* bit_util_suite(void) {
  Suite* suite = suite_create("bit_util");

  TCase* endian_tcase = tcase_create("endian");
  tcase_add_test(endian_tcase, test_write_big_endian);
  tcase_add_test(endian_tcase, test_read_big_endian);
  suite_add_tcase(suite, endian_tcase);

  return suite;
}
