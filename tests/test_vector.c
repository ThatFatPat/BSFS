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

Suite* vector_suite(void) {
  Suite* suite = suite_create("vector");

  TCase* scalar_product_tcase = tcase_create("scalar_product");
  tcase_add_test(scalar_product_tcase, test_scalar_product);
  suite_add_tcase(suite, scalar_product_tcase);

  TCase* linear_combination_tcase = tcase_create("linear_combination");
  tcase_add_test(linear_combination_tcase, test_linear_combination);
  suite_add_tcase(suite, linear_combination_tcase);

  return suite;
}
