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
  // TODO: Make sure file doesn't exist!
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

  ck_assert_int_eq(
      bsfs_mknod(tmp_fs, "/getattrlvl/file1", S_IFREG | S_IRUSR | S_IWUSR), 0);
}

static void getattr_fs_teardown(void) {
  bsfs_destroy(tmp_fs);
}

START_TEST(test_getattr) {
  struct stat st;
  ck_assert_int_eq(bsfs_getattr(tmp_fs, "getattrlvl/file1", &st), 0);

  ck_assert_uint_eq(st.st_size, 0);
  ck_assert_int_eq(st.st_mode, S_IFREG | S_IRUSR | S_IWUSR);
  ck_assert_int_eq(st.st_nlink, 1);
}
END_TEST

START_TEST(test_getattr_noent) {
  struct stat st;
  ck_assert_int_eq(bsfs_getattr(tmp_fs, "getattrlvl/asjkdl", &st), -ENOENT);
}
END_TEST

START_TEST(test_fgetattr) {
  bs_file_t file;
  ck_assert_int_eq(bsfs_open(tmp_fs, "getattrlvl/file1", &file), 0);

  struct stat st;
  ck_assert_int_eq(bsfs_fgetattr(file, &st), 0);

  ck_assert_uint_eq(st.st_size, 0);
  ck_assert_int_eq(st.st_mode, S_IFREG | S_IRUSR | S_IWUSR);
  ck_assert_int_eq(st.st_nlink, 1);

  ck_assert_int_eq(bsfs_release(file), 0);
}
END_TEST

static stego_key_t chmod_key;

static void chmod_fs_setup(void) {
  int fd = create_tmp_file(FS_DISK_SIZE);
  ck_assert_int_eq(bsfs_init(fd, &tmp_fs), 0);

  ck_assert_int_eq(stego_gen_user_keys(&chmod_key, 1), 0);
  ck_assert_int_eq(keytab_store(tmp_fs->disk, 0, "chmodlvl", &chmod_key), 0);

  void* zero = calloc(1, BFT_SIZE);
  ck_assert(zero);
  ck_assert_int_eq(bft_write_table(&chmod_key, tmp_fs->disk, zero), 0);
  ck_assert_int_eq(fs_write_bitmap(&chmod_key, tmp_fs->disk, zero), 0);
  free(zero);

  ck_assert_int_eq(
      bsfs_mknod(tmp_fs, "/chmodlvl/file1", S_IFREG | S_IRUSR | S_IWUSR), 0);
}

static void chmod_fs_teardown(void) {
  bsfs_destroy(tmp_fs);
}

START_TEST(test_chmod) {
  struct stat st;
  ck_assert_int_eq(bsfs_getattr(tmp_fs, "/chmodlvl/file1", &st), 0);
  ck_assert_uint_eq(st.st_mode, S_IFREG | S_IRUSR | S_IWUSR);

  ck_assert_int_eq(bsfs_chmod(tmp_fs, "/chmodlvl/file1", S_IFREG | S_IRUSR), 0);
  ck_assert_int_eq(bsfs_getattr(tmp_fs, "/chmodlvl/file1", &st), 0);
  ck_assert_uint_eq(st.st_mode, S_IFREG | S_IRUSR);
}
END_TEST

START_TEST(test_fchmod) {
  bs_file_t file;
  ck_assert_int_eq(bsfs_open(tmp_fs, "/chmodlvl/file1", &file), 0);

  struct stat st;
  ck_assert_int_eq(bsfs_fgetattr(file, &st), 0);
  ck_assert_uint_eq(st.st_mode, S_IFREG | S_IRUSR | S_IWUSR);

  ck_assert_int_eq(bsfs_fchmod(file, S_IFREG | S_IRUSR), 0);
  ck_assert_int_eq(bsfs_fgetattr(file, &st), 0);
  ck_assert_uint_eq(st.st_mode, S_IFREG | S_IRUSR);

  ck_assert_int_eq(bsfs_release(file), 0);
}
END_TEST

START_TEST(test_chmod_file_type) {
  struct stat st;
  ck_assert_int_eq(bsfs_getattr(tmp_fs, "/chmodlvl/file1", &st), 0);
  ck_assert_uint_eq(st.st_mode, S_IFREG | S_IRUSR | S_IWUSR);

  ck_assert_int_eq(bsfs_chmod(tmp_fs, "/chmodlvl/file1", S_IFDIR | S_IRUSR), 0);
  ck_assert_int_eq(bsfs_getattr(tmp_fs, "/chmodlvl/file1", &st), 0);
  ck_assert_uint_eq(st.st_mode, S_IFREG | S_IRUSR);
}
END_TEST

stego_key_t rename_key;
const char* rename_level_name = "renamelvl";

static void rename_fs_setup(void) {
  int fd = create_tmp_file(FS_DISK_SIZE + 0x4000);
  ck_assert_int_eq(bsfs_init(fd, &tmp_fs), 0);

  ck_assert_int_eq(stego_gen_user_keys(&rename_key, 1), 0);
  ck_assert_int_eq(
      keytab_store(tmp_fs->disk, 0, rename_level_name, &rename_key), 0);

  void* zero = calloc(1, BFT_SIZE);
  ck_assert(zero);
  ck_assert_int_eq(bft_write_table(&rename_key, tmp_fs->disk, zero), 0);
  ck_assert_int_eq(fs_write_bitmap(&rename_key, tmp_fs->disk, zero), 0);
  free(zero);

  char path[256];
  strcpy(path, rename_level_name);
  ck_assert_int_eq(bsfs_mknod(tmp_fs, strcat(path, "/bla"), S_IFREG), 0);
}

static void rename_fs_teardown(void) {
  bsfs_destroy(tmp_fs);
}

START_TEST(test_rename) {
  char path[256];
  strcpy(path, rename_level_name);
  strcat(path, "/bla");

  char new_path[256];
  strcpy(new_path, rename_level_name);
  strcat(new_path, "/bla1");

  ck_assert_int_eq(bsfs_rename(tmp_fs, path, new_path, 0), 0);

  bs_file_t old_file;
  ck_assert_int_eq(bsfs_open(tmp_fs, path, &old_file), -ENOENT);

  bs_file_t rename_file;
  ck_assert_int_eq(bsfs_open(tmp_fs, new_path, &rename_file), 0);
  ck_assert_int_eq(bsfs_release(rename_file), 0);
}
END_TEST

START_TEST(test_rename_noent) {
  char path[256];
  strcpy(path, rename_level_name);
  strcat(path, "/blabla");

  char new_path[256];
  strcpy(new_path, rename_level_name);
  strcat(new_path, "/bla1");

  ck_assert_int_eq(bsfs_rename(tmp_fs, path, new_path, 0), -ENOENT);
}
END_TEST

START_TEST(test_rename_cross_level) {
  ck_assert_int_eq(bsfs_rename(tmp_fs, "renamelvl/file1", "otherlvl/file1", 0),
                   -EXDEV);
}
END_TEST

START_TEST(test_rename_noreplace) {
  char path[256];
  strcpy(path, rename_level_name);
  strcat(path, "/bla");

  char new_path[256];
  strcpy(new_path, rename_level_name);
  strcat(new_path, "/bla1");

  ck_assert_int_eq(bsfs_mknod(tmp_fs, new_path, S_IFREG), 0);
  ck_assert_int_eq(bsfs_rename(tmp_fs, path, new_path, RENAME_NOREPLACE),
                   -EEXIST);
}
END_TEST

START_TEST(test_rename_invalid_flags) {
  char path[256];
  strcpy(path, rename_level_name);
  strcat(path, "/bla");

  char new_path[256];
  strcpy(new_path, rename_level_name);
  strcat(new_path, "/bla1");

  ck_assert_int_eq(bsfs_rename(tmp_fs, path, new_path, 0x1234), -EINVAL);
}
END_TEST

START_TEST(test_rename_replace) {
  char path[256];
  strcpy(path, rename_level_name);
  strcat(path, "/bla");

  char new_path[256];
  strcpy(new_path, rename_level_name);
  strcat(new_path, "/bla1");

  ck_assert_int_eq(bsfs_mknod(tmp_fs, new_path, S_IFREG | S_IRUSR), 0);
  ck_assert_int_eq(bsfs_rename(tmp_fs, path, new_path, 0), 0);

  bs_file_t old_file;
  ck_assert_int_eq(bsfs_open(tmp_fs, path, &old_file), -ENOENT);

  struct stat st;
  ck_assert_int_eq(bsfs_getattr(tmp_fs, new_path, &st), 0);
  ck_assert_int_eq(st.st_mode, S_IFREG);
}
END_TEST

START_TEST(test_rename_exchange) {
  char path[256];
  strcpy(path, rename_level_name);
  strcat(path, "/bla");

  char new_path[256];
  strcpy(new_path, rename_level_name);
  strcat(new_path, "/bla1");

  ck_assert_int_eq(bsfs_mknod(tmp_fs, new_path, S_IFREG | S_IRUSR), 0);
  ck_assert_int_eq(bsfs_rename(tmp_fs, path, new_path, RENAME_EXCHANGE), 0);

  struct stat st_old;
  ck_assert_int_eq(bsfs_getattr(tmp_fs, path, &st_old), 0);
  ck_assert_int_eq(st_old.st_mode, S_IFREG | S_IRUSR);

  struct stat st_new;
  ck_assert_int_eq(bsfs_getattr(tmp_fs, new_path, &st_new), 0);
  ck_assert_int_eq(st_new.st_mode, S_IFREG);
}
END_TEST

START_TEST(test_rename_exchange_no_dest) {
  char path[256];
  strcpy(path, rename_level_name);
  strcat(path, "/bla");

  char new_path[256];
  strcpy(new_path, rename_level_name);
  strcat(new_path, "/bla1");

  ck_assert_int_eq(bsfs_rename(tmp_fs, path, new_path, RENAME_EXCHANGE),
                   -ENOENT);
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
  tcase_add_test(getattr_tcase, test_getattr_noent);
  tcase_add_test(getattr_tcase, test_fgetattr);
  suite_add_tcase(suite, getattr_tcase);

  TCase* chmod_tcase = tcase_create("chmod");
  tcase_add_checked_fixture(chmod_tcase, chmod_fs_setup, chmod_fs_teardown);
  tcase_add_test(chmod_tcase, test_chmod);
  tcase_add_test(chmod_tcase, test_fchmod);
  tcase_add_test(chmod_tcase, test_chmod_file_type);
  suite_add_tcase(suite, chmod_tcase);

  TCase* rename_tcase = tcase_create("rename");
  tcase_add_checked_fixture(rename_tcase, rename_fs_setup, rename_fs_teardown);
  tcase_add_test(rename_tcase, test_rename);
  tcase_add_test(rename_tcase, test_rename_noent);
  tcase_add_test(rename_tcase, test_rename_cross_level);
  tcase_add_test(rename_tcase, test_rename_noreplace);
  tcase_add_test(rename_tcase, test_rename_invalid_flags);
  tcase_add_test(rename_tcase, test_rename_replace);
  tcase_add_test(rename_tcase, test_rename_exchange);
  tcase_add_test(rename_tcase, test_rename_exchange_no_dest);
  suite_add_tcase(suite, rename_tcase);

  return suite;
}
