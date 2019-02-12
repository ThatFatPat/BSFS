#include "test_matrix.h"

#include "matrix.h"
#include <errno.h>
#include <stdint.h>
#include <string.h>

START_TEST(test_matrix_transpose) {
  uint8_t mat[8] = { 0xe2, 0xf2, 0x1c, 0x77, 0x19, 0x4e, 0x7b, 0xe1 };
  uint8_t expected_transpose[8] = { 0xc1, 0xd7, 0xd3, 0x7a,
                                    0x2e, 0x34, 0xd6, 0x1b };
  uint8_t transpose[8];

  matrix_transpose(transpose, mat, 8);
  matrix_transpose(mat, mat, 8);

  ck_assert_int_eq(memcmp(transpose, expected_transpose, 8), 0);
  ck_assert_int_eq(memcmp(transpose, mat, 8), 0);
}
END_TEST

START_TEST(test_matrix_multiplication) {
  uint8_t mat1[8] = { 0xe2, 0xf2, 0x1c, 0x77, 0x19, 0x4e, 0x7b, 0xe1 };
  uint8_t mat2[8] = { 0xe1, 0x91, 0x76, 0x8, 0xad, 0xc6, 0xe0, 0x4c };
  uint8_t expected_mul[8] = { 0xe6, 0xee, 0x63, 0x85, 0xe9, 0x1a, 0xee, 0x4a };
  uint8_t mul[8];

  matrix_multiply(mul, mat1, mat2, 8);

  ck_assert_int_eq(memcmp(mul, expected_mul, 8), 0);
}
END_TEST

START_TEST(test_matrix_invert) {
  const uint8_t identity[2] = { 0x84, 0x21 };
  const uint8_t matrix[2] = { 0x16, 0x9a };
  uint8_t inverse[2];
  uint8_t product[2];

  ck_assert_int_eq(matrix_invert(inverse, matrix, 4), 0);
  matrix_multiply(product, matrix, inverse, 4);
  ck_assert_int_eq(memcmp(product, identity, sizeof(identity)), 0);
}
END_TEST

START_TEST(test_matrix_invert_identity) {
  const uint8_t identity[8] = { 0x80, 0x40, 0x20, 0x10, 0x8, 0x4, 0x2, 0x1 };
  uint8_t inverse[8];
  ck_assert_int_eq(matrix_invert(inverse, identity, 8), 0);
  ck_assert_int_eq(memcmp(inverse, identity, sizeof(identity)), 0);
}
END_TEST

START_TEST(test_matrix_invert_singular) {
  uint8_t zero[8] = { 0 };
  uint8_t inverse[8];
  ck_assert_int_eq(matrix_invert(inverse, zero, 8), -EINVAL);
}
END_TEST

START_TEST(test_matrix_gen_nonsingular) {
  size_t dim = 8;

  uint8_t mat[8] = { 0x80, 0x40, 0x20, 0x10, 0x8, 0x4, 0x2, 0x1 };
  uint8_t inverse[8];

  ck_assert_int_eq(matrix_gen_nonsing(mat, dim), 0);

  ck_assert_int_eq(matrix_invert(inverse, mat, 8), 0);
}
END_TEST

Suite* matrix_suite(void) {
  Suite* suite = suite_create("matrix");

  TCase* matrix_transpose_tcase = tcase_create("matrix_transpose");
  tcase_add_test(matrix_transpose_tcase, test_matrix_transpose);
  suite_add_tcase(suite, matrix_transpose_tcase);

  TCase* matrix_multiply_tcase = tcase_create("matrix_multiply");
  tcase_add_test(matrix_multiply_tcase, test_matrix_multiplication);
  suite_add_tcase(suite, matrix_multiply_tcase);

  TCase* matrix_nonsing_tcase = tcase_create("nonsing");
  tcase_add_test(matrix_nonsing_tcase, test_matrix_invert);
  tcase_add_test(matrix_nonsing_tcase, test_matrix_invert_identity);
  tcase_add_test(matrix_nonsing_tcase, test_matrix_invert_singular);
  tcase_add_test(matrix_nonsing_tcase, test_matrix_gen_nonsingular);
  suite_add_tcase(suite, matrix_nonsing_tcase);

  return suite;
}
