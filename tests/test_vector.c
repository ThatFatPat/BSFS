#include "test_vector.h"

#include "vector.h"
#include <stdint.h>

START_TEST(test_scalar_product) {
  uint32_t vector1 = 0x3f1fee11;
  uint32_t vector2 = 0x1e2f2387;

  ck_assert(vector_scalar_product((const_vector_t) &vector1,
                                  (const_vector_t) &vector2, sizeof(uint32_t)));

  vector1 &= ~((uint32_t) 1);

  ck_assert(!vector_scalar_product(
      (const_vector_t) &vector1, (const_vector_t) &vector2, sizeof(uint32_t)));
}
END_TEST

START_TEST(test_norm) {
  uint32_t vector = -1;
  ck_assert(!vector_norm((const_vector_t) &vector, sizeof(uint32_t)));

  vector &= ~((uint32_t) 1);

  ck_assert(vector_norm((const_vector_t) &vector, sizeof(uint32_t)));
}
END_TEST

START_TEST(test_linear_combination) {
  uint64_t vector1 = 0xdeadbeefdeadc0de;
  uint64_t vector2 = 0x8badf00dc0decafe;

  uint64_t linear_combination;

  vector_linear_combination((vector_t) &linear_combination, (vector_t) &vector1,
                            (vector_t) &vector2, sizeof(uint64_t), 0);
  ck_assert_uint_eq(linear_combination, vector1);

  vector_linear_combination((vector_t) &linear_combination, (vector_t) &vector1,
                            (vector_t) &vector2, sizeof(uint64_t), 1);
  ck_assert_uint_eq(linear_combination, vector1 ^ vector2);
}
END_TEST

START_TEST(test_matrix_multiplication) {
  uint8_t mat1[8] = { 0xe2, 0xf2, 0x1c, 0x77, 0x19, 0x4e, 0x7b, 0xe1 };
  uint8_t mat2[8] = { 0xe1, 0x91, 0x76, 0x8, 0xad, 0xc6, 0xe0, 0x4c };
  uint8_t expected_mul[8] = { 0xe6, 0xee, 0x63, 0x85, 0xe9, 0x1a, 0xee, 0x4a };
  uint8_t mul[8];

  matrix_multiply(mul, mat1, mat2, 8);

  ck_assert_uint_eq(memcmp(mul, expected_mul, 8), 0);
}
END_TEST

START_TEST(test_matrix_transpose) {
  uint8_t mat[8] = { 0xe2, 0xf2, 0x1c, 0x77, 0x19, 0x4e, 0x7b, 0xe1 };
  uint8_t expected_transpose[8] = { 0xc1, 0xd7, 0xd3, 0x7a,
                                    0x2e, 0x34, 0xd6, 0x1b };
  uint8_t transpose[8];

  matrix_transpose(transpose, mat, 8);
  matrix_transpose(mat, mat, 8);

  ck_assert_uint_eq(memcmp(transpose, expected_transpose, 8), 0);
  ck_assert_uint_eq(memcmp(transpose, mat, 8), 0);
}
END_TEST

START_TEST(test_matrix_gen_nonsing) {
  uint8_t mat[8];
  uint8_t inv[8];
  uint8_t mul1[8];
  uint8_t mul2[8];
  uint8_t expected_mul[8] = { 0x80, 0x40, 0x20, 0x10, 0x8, 0x4, 0x2, 0x1 };

  ck_assert_uint_eq(matrix_gen_nonsing(mat, inv, 8), 0);

  matrix_multiply(mul1, inv, mat, 8);
  matrix_multiply(mul2, mat, inv, 8);

  ck_assert_uint_eq(memcmp(mul1, expected_mul, 8), 0);
  ck_assert_uint_eq(memcmp(mul2, expected_mul, 8), 0);
}
END_TEST

Suite* vector_suite(void) {
  Suite* suite = suite_create("vector");

  TCase* scalar_product_tcase = tcase_create("scalar_product");
  tcase_add_test(scalar_product_tcase, test_scalar_product);
  tcase_add_test(scalar_product_tcase, test_norm);
  suite_add_tcase(suite, scalar_product_tcase);

  TCase* linear_combination_tcase = tcase_create("linear_combination");
  tcase_add_test(linear_combination_tcase, test_linear_combination);
  suite_add_tcase(suite, linear_combination_tcase);

  TCase* matrix_multiply_tcase = tcase_create("matrix_multiply");
  tcase_add_test(matrix_multiply_tcase, test_matrix_multiplication);
  suite_add_tcase(suite, matrix_multiply_tcase);

  TCase* matrix_transpose_tcase = tcase_create("matrix_transpose");
  tcase_add_test(matrix_transpose_tcase, test_matrix_transpose);
  suite_add_tcase(suite, matrix_transpose_tcase);

  TCase* matrix_gen_nonsing_tcase = tcase_create("matrix_gen_nonsing");
  tcase_add_test(matrix_gen_nonsing_tcase, test_matrix_gen_nonsing);
  suite_add_tcase(suite, matrix_gen_nonsing_tcase);

  return suite;
}
