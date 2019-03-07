#include "test_bsfs.h"

#include "bft.h"
#include "bsfs_priv.h"
#include "keytab.h"
#include "stego.h"
#include <errno.h>
#include <sys/syscall.h>
#include <unistd.h>

#define FS_DISK_SIZE                                                           \
  (KEYTAB_SIZE + STEGO_USER_LEVEL_COUNT * (BFT_SIZE + 0x1000))

static int create_tmp_file(size_t size) {
  int fd = syscall(SYS_memfd_create, "test_bsfs.bsf", 0);
  ck_assert_int_ne(ftruncate(fd, size), -1);
  return fd;
}

START_TEST(test_bsfs_init) {
  int fd = create_tmp_file(FS_DISK_SIZE);
  bs_bsfs_t fs;
  ck_assert_int_eq(bsfs_init(fd, &fs), 0);
  bsfs_destroy(fs);
}
END_TEST

START_TEST(test_bsfs_init_disk_too_small) {
  int fd = create_tmp_file(5);
  bs_bsfs_t fs;
  ck_assert_int_eq(bsfs_init(fd, &fs), -ENOSPC);
  close(fd);
}
END_TEST

static bs_bsfs_t tmp_fs;

stego_key_t level_get_keys[2];
static const char* level_get_pass1 = "pass1";
static const char* level_get_pass2 = "pass2";

static void level_get_fs_setup(void) {
  ck_assert_int_eq(stego_gen_user_keys(level_get_keys, 2), 0);
  int fd = create_tmp_file(FS_DISK_SIZE);
  ck_assert_int_eq(bsfs_init(fd, &tmp_fs), 0);
  ck_assert_int_eq(
      keytab_store(tmp_fs->disk, 0, level_get_pass1, level_get_keys), 0);
  ck_assert_int_eq(
      keytab_store(tmp_fs->disk, 1, level_get_pass2, level_get_keys + 1), 0);
}

static void level_get_fs_teardown(void) {
  bsfs_destroy(tmp_fs);
}

START_TEST(test_level_get) {
  bs_open_level_t level1;
  ck_assert_int_eq(bs_level_get(tmp_fs, level_get_pass1, &level1), 0);
  ck_assert_int_eq(memcmp(&level1->key, level_get_keys, sizeof(stego_key_t)),
                   0);
  ck_assert_str_eq(level1->pass, level_get_pass1);
}
END_TEST

Suite* bsfs_suite(void) {
  Suite* suite = suite_create("bsfs");

  TCase* init_tcase = tcase_create("init");
  tcase_add_test(init_tcase, test_bsfs_init);
  tcase_add_test(init_tcase, test_bsfs_init_disk_too_small);
  suite_add_tcase(suite, init_tcase);

  TCase* level_get_tcase = tcase_create("level_get");
  tcase_add_checked_fixture(level_get_tcase, level_get_fs_setup,
                            level_get_fs_teardown);
  tcase_add_test(level_get_tcase, test_level_get);
  suite_add_tcase(suite, level_get_tcase);

  return suite;
}
