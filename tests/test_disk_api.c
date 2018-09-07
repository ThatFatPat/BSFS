#include "test_disk_api.h"
#include "disk_api.h"
#include <stdio.h>

START_TEST(test_disk_create)
{
  bs_disk_t* disk;
  FILE *fptr;
  fptr = fopen("../res/data.bsf", "rw");
  ck_assert(fptr != NULL); // Make sure file at res/data.bsf opens and returns file pointer.
  int ret = disk_create(fileno(fptr), disk);
  ck_assert_int_eq(ret, 0);
}
END_TEST

Suite* disk_api_suite(void) {
  Suite* suite = suite_create("disk_api");
  TCase* disk_create_tcase = tcase_create("test_disk_create");

  tcase_add_test(disk_create_tcase, test_disk_create);
  suite_add_tcase(suite, disk_create_tcase);

  return suite;
}
