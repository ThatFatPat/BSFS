#include "test_disk_api.h"

START_TEST(disk_create)
{
  ck_assert_int_eq(3, 3);
}
END_TEST

Suite* disk_api_suite(void) {
  Suite* suite = suite_create("disk_api");
  TCase* example_tcase = tcase_create("example");

  tcase_add_test(example_tcase, test_example);
  suite_add_tcase(suite, example_tcase);

  return suite;
}
