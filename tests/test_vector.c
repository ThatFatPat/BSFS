#include "test_vector.h"

#include "vector.h"

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

Suite* vector_suite(void) {
  Suite* suite = suite_create("vector");

  TCase* linear_combination_tcase = tcase_create("linear_combination");
  tcase_add_test(linear_combination_tcase, test_linear_combination);
  suite_add_tcase(suite, linear_combination_tcase);

  return suite;
}
