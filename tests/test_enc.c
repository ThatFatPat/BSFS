#include "test_enc.h"

START_TEST(test_example)
{
  ck_assert_int_eq(3, 3);
}
END_TEST

Suite* enc_suite(void) {
  Suite* suite = suite_create("enc");
  TCase* example_tcase = tcase_create("example");

  tcase_add_test(example_tcase, test_example);
  suite_add_tcase(suite, example_tcase);

  return suite;
}
