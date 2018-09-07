#include "test_disk_api.h"
#include "disk_api.h"
#include <stdio.h>
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>

#define FILEPATH "../res/data.bsf"

START_TEST(test_disk_create)
{
  bs_disk_t* disk;
  int fptr;
  fptr = open(FILEPATH, O_RDWR);
  ck_assert(fptr != -1); // Make sure file at res/data.bsf opens and returns file pointer.
  ck_assert_int_eq(disk_create(fptr, disk), 0);
  close(fptr);
}
END_TEST

Suite* disk_api_suite(void) {
  Suite* suite = suite_create("disk_api");
  TCase* disk_create_tcase = tcase_create("test_disk_create");

  tcase_add_test(disk_create_tcase, test_disk_create);
  suite_add_tcase(suite, disk_create_tcase);

  return suite;
}
