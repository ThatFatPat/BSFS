#include "test_bsfs.h"

#include "bft.h"
#include "bsfs_priv.h"
#include "keytab.h"
#include "stego.h"
#include <errno.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <unistd.h>

#define FS_DISK_SIZE                                                           \
  (KEYTAB_SIZE + STEGO_USER_LEVEL_COUNT * (BFT_SIZE + 0x2000))

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
  int fd = create_tmp_file(FS_DISK_SIZE);
  ck_assert_int_eq(bsfs_init(fd, &tmp_fs), 0);

  ck_assert_int_eq(stego_gen_user_keys(level_get_keys, 2), 0);
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

START_TEST(test_level_get_noent) {
  bs_open_level_t level;
  ck_assert_int_eq(bs_level_get(tmp_fs, "sjklfajklsdjfklj2wioej", &level),
                   -ENOENT);
}
END_TEST

START_TEST(test_level_get_twice) {
  bs_open_level_t level1;
  bs_open_level_t level2;
  ck_assert_int_eq(bs_level_get(tmp_fs, level_get_pass1, &level1), 0);
  ck_assert_int_eq(bs_level_get(tmp_fs, level_get_pass1, &level2), 0);
  ck_assert_ptr_eq(level1, level2);
}
END_TEST

START_TEST(test_level_get_multi) {
  bs_open_level_t level1;
  bs_open_level_t level2;
  ck_assert_int_eq(bs_level_get(tmp_fs, level_get_pass1, &level1), 0);
  ck_assert_int_eq(bs_level_get(tmp_fs, level_get_pass2, &level2), 0);
  ck_assert_ptr_ne(level1, level2);
}
END_TEST

START_TEST(test_split_path) {
  const char* path = "/pass/name";
  char* pass;
  char* name;

  ck_assert_int_eq(bs_split_path(path, &pass, &name), 0);
  ck_assert_str_eq(pass, "pass");
  ck_assert_str_eq(name, "name");

  free(pass);
  free(name);

  ck_assert_int_eq(bs_split_path(path + 1, &pass, &name), 0);
  ck_assert_str_eq(pass, "pass");
  ck_assert_str_eq(name, "name");

  free(pass);
  free(name);
}
END_TEST

START_TEST(test_split_path_no_slash) {
  const char* path = "/passname";
  char* pass;
  char* name;

  ck_assert_int_eq(bs_split_path(path, &pass, &name), -ENOTSUP);
  ck_assert_int_eq(bs_split_path(path + 1, &pass, &name), -ENOTSUP);
}
END_TEST

START_TEST(test_split_path_double_slash) {
  const char* path = "/pass/nam/e";
  char* pass;
  char* name;

  ck_assert_int_eq(bs_split_path(path, &pass, &name), -ENOTSUP);
  ck_assert_int_eq(bs_split_path(path + 1, &pass, &name), -ENOTSUP);
}
END_TEST

stego_key_t mknod_key;
const char* mknod_level_name = "mknodlvl";

static void mknod_fs_setup(void) {
  int fd = create_tmp_file(FS_DISK_SIZE);
  ck_assert_int_eq(bsfs_init(fd, &tmp_fs), 0);

  ck_assert_int_eq(stego_gen_user_keys(&mknod_key, 1), 0);
  ck_assert_int_eq(keytab_store(tmp_fs->disk, 0, mknod_level_name, &mknod_key),
                   0);

  void* zero = calloc(1, BFT_SIZE);
  ck_assert(zero);
  ck_assert_int_eq(bft_write_table(&mknod_key, tmp_fs->disk, zero), 0);
  ck_assert_int_eq(fs_write_bitmap(&mknod_key, tmp_fs->disk, zero), 0);
  free(zero);
}

static void mknod_fs_teardown(void) {
  bsfs_destroy(tmp_fs);
}

START_TEST(test_mknod) {
  char path[256];
  strcpy(path, mknod_level_name);
  ck_assert_int_eq(bsfs_mknod(tmp_fs, strcat(path, "/bla"), S_IFREG), 0);
}
END_TEST

START_TEST(test_mknod_not_reg) {
  char path[256];
  strcpy(path, mknod_level_name);
  ck_assert_int_eq(bsfs_mknod(tmp_fs, strcat(path, "/fifo"), S_IFIFO),
                   -ENOTSUP);
}
END_TEST

START_TEST(test_mknod_no_such_level) {
  const char* path = "/nosuchlvl/file";
  ck_assert_int_eq(bsfs_mknod(tmp_fs, path, S_IFREG), -ENOENT);
}
END_TEST

START_TEST(test_unlink) {
  char path[256];
  strcpy(path, mknod_level_name);
  strcat(path, "/bla");
  ck_assert_int_eq(bsfs_mknod(tmp_fs, path, S_IFREG), 0);
  ck_assert_int_eq(bsfs_unlink(tmp_fs, path), 0);
}
END_TEST

START_TEST(test_unlink_noent) {
  char path[256];
  strcpy(path, mknod_level_name);
  strcat(path, "/bla");
  ck_assert_int_eq(bsfs_unlink(tmp_fs, path), -ENOENT);
}
END_TEST

static stego_key_t getattr_key;
static const char* getattr_level_name = "getattrlvl";

static void getattr_fs_setup(void) {
  int fd = create_tmp_file(FS_DISK_SIZE);
  ck_assert_int_eq(bsfs_init(fd, &tmp_fs), 0);

  ck_assert_int_eq(stego_gen_user_keys(&getattr_key, 1), 0);
  ck_assert_int_eq(
      keytab_store(tmp_fs->disk, 0, getattr_level_name, &getattr_key), 0);

  void* zero = calloc(1, BFT_SIZE);
  ck_assert(zero);
  ck_assert_int_eq(bft_write_table(&getattr_key, tmp_fs->disk, zero), 0);
  ck_assert_int_eq(fs_write_bitmap(&getattr_key, tmp_fs->disk, zero), 0);
  free(zero);

  ck_assert_int_eq(bsfs_mknod(tmp_fs, "/getattrlvl/file1", S_IFREG), 0);
  ck_assert_int_eq(
      bsfs_mknod(tmp_fs, "/getattrlvl/file2", S_IFREG | S_IRUSR | S_IWUSR), 0);
}

static void getattr_fs_teardown(void) {
  bsfs_destroy(tmp_fs);
}

START_TEST(test_getattr) {
  struct stat st;
  ck_assert_int_eq(bsfs_getattr(tmp_fs, "getattrlvl/file1", &st), 0);

  ck_assert_uint_eq(st.st_size, 0);
  ck_assert_int_eq(st.st_mode, S_IFREG);
  ck_assert_int_eq(st.st_nlink, 1);
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
  tcase_add_test(level_get_tcase, test_level_get_noent);
  tcase_add_test(level_get_tcase, test_level_get_twice);
  tcase_add_test(level_get_tcase, test_level_get_multi);
  suite_add_tcase(suite, level_get_tcase);

  TCase* split_path_tcase = tcase_create("split_path");
  tcase_add_test(split_path_tcase, test_split_path);
  tcase_add_test(split_path_tcase, test_split_path_no_slash);
  tcase_add_test(split_path_tcase, test_split_path_double_slash);
  suite_add_tcase(suite, split_path_tcase);

  TCase* mknod_unlink_tcase = tcase_create("mknod_unlink");
  tcase_add_checked_fixture(mknod_unlink_tcase, mknod_fs_setup,
                            mknod_fs_teardown);
  tcase_add_test(mknod_unlink_tcase, test_mknod);
  tcase_add_test(mknod_unlink_tcase, test_mknod_not_reg);
  tcase_add_test(mknod_unlink_tcase, test_mknod_no_such_level);
  tcase_add_test(mknod_unlink_tcase, test_unlink);
  tcase_add_test(mknod_unlink_tcase, test_unlink_noent);
  suite_add_tcase(suite, mknod_unlink_tcase);

  TCase* getattr_tcase = tcase_create("getattr");
  tcase_add_checked_fixture(getattr_tcase, getattr_fs_setup,
                            getattr_fs_teardown);
  tcase_add_test(getattr_tcase, test_getattr);
  suite_add_tcase(suite, getattr_tcase);

  return suite;
}
