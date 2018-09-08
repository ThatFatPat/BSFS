#include "test_disk.h"

#include "disk.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/syscall.h>
#include <unistd.h>

static int open_tmp_file(){
  int fptr = syscall(SYS_memfd_create, "data.bsf", 0);
  char content[] = "Hello, I'm the Doctor.\n Basically, Run.\n";
  write(fptr, content, sizeof(content));
  return fptr;
}

START_TEST(test_disk_create)
{
  bs_disk_t disk;
  int fptr = open_tmp_file();
  ck_assert_int_eq(disk_create(fptr, &disk), 0);
  disk_free(disk);
}
END_TEST

START_TEST(test_disk_roundtrip)
{
  bs_disk_t disk;
  int fptr = open_tmp_file();
  ck_assert_int_eq(disk_create(fptr, &disk), 0);
  char write_text[] = "This is a shorter text.";
  void* writeable_disk;
  ck_assert_int_eq(disk_lock_write(disk, &writeable_disk), 0);
  strcpy((char*)writeable_disk, write_text);
  ck_assert_str_eq((char*)writeable_disk, write_text);
  ck_assert_int_eq(disk_unlock_write(disk), 0);
  const void* data;
  ck_assert_int_eq(disk_lock_read(disk, &data), 0);
  ck_assert_str_eq((const char*)data, write_text);
  ck_assert_int_eq(disk_unlock_read(disk), 0);
  
  disk_free(disk);
}
END_TEST

Suite* disk_suite(void) {
  Suite* suite = suite_create("disk_api");
  TCase* disk_create_tcase = tcase_create("test_disk_create");
  TCase* disk_roundtrip_tcase = tcase_create("test_disk_roundtrip");

  tcase_add_test(disk_create_tcase, test_disk_create);
  tcase_add_test(disk_roundtrip_tcase, test_disk_roundtrip);
  suite_add_tcase(suite, disk_create_tcase);
  suite_add_tcase(suite, disk_roundtrip_tcase);

  return suite;
}
