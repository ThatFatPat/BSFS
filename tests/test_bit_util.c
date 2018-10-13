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

START_TEST(test_set_bit) {
  uint8_t buf[] = { 0xde, 0xad, 0xbe, 0xef };
  set_bit(buf, 7, 1);
  ck_assert_int_eq(memcmp(buf, "\xdf\xad\xbe\xef", sizeof(buf)), 0);
  set_bit(buf, 8, 0);
  ck_assert_int_eq(memcmp(buf, "\xdf\x2d\xbe\xef", sizeof(buf)), 0);
}
END_TEST

START_TEST(test_get_bit) {
  uint8_t buf[] = { 0xde, 0xad, 0xbe, 0xef };
  ck_assert(!get_bit(buf, 7));
  ck_assert(get_bit(buf, 16));
  ck_assert(get_bit(buf, 31));
}
END_TEST

START_TEST(test_get_set_bit_roundtrip) {
  uint8_t buf[16] = { 0 };
  ck_assert(!get_bit(buf, 123));
  set_bit(buf, 123, 1);
  ck_assert(get_bit(buf, 123));
  set_bit(buf, 123, 0);
  ck_assert(!get_bit(buf, 123));
}
END_TEST

Suite* bit_util_suite(void) {
  Suite* suite = suite_create("bit_util");

  TCase* endian_tcase = tcase_create("endian");
  tcase_add_test(endian_tcase, test_write_big_endian);
  tcase_add_test(endian_tcase, test_read_big_endian);
  suite_add_tcase(suite, endian_tcase);

  TCase* bit_tcase = tcase_create("bits");
  tcase_add_test(bit_tcase, test_set_bit);
  tcase_add_test(bit_tcase, test_get_bit);
  tcase_add_test(bit_tcase, test_get_set_bit_roundtrip);
  suite_add_tcase(suite, bit_tcase);

  return suite;
}
