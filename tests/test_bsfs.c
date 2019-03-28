#include "test_bsfs.h"

#include "bft.h"
#include "bsfs_priv.h"
#include "keytab.h"
#include "stego.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>

#define FS_DISK_SIZE                                                           \
  (KEYTAB_SIZE + STEGO_USER_LEVEL_COUNT * (BFT_SIZE + 0x4000))

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

START_TEST(test_get_dirname) {
  const char* path = "/pass";
  char* pass;

  ck_assert_int_eq(bs_get_dirname(path, &pass), 0);
  ck_assert_str_eq(pass, "pass");

  free(pass);

  ck_assert_int_eq(bs_get_dirname(path + 1, &pass), 0);
  ck_assert_str_eq(pass, "pass");

  free(pass);
}
END_TEST

START_TEST(test_get_dirname_trailing_slash) {
  const char* path = "/pass/";
  char* pass;

  ck_assert_int_eq(bs_get_dirname(path, &pass), 0);
  ck_assert_str_eq(pass, "pass");

  free(pass);

  ck_assert_int_eq(bs_get_dirname(path + 1, &pass), 0);
  ck_assert_str_eq(pass, "pass");

  free(pass);
}
END_TEST

START_TEST(test_get_dirname_with_file) {
  const char* path = "/pass/name";
  char* pass;
  ck_assert_int_eq(bs_get_dirname(path, &pass), -ENOTDIR);
  ck_assert_int_eq(bs_get_dirname(path + 1, &pass), -ENOTDIR);
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

START_TEST(test_getattr_root) {
  struct stat st;
  ck_assert_int_eq(bsfs_getattr(tmp_fs, "/", &st), 0);
  ck_assert_uint_eq(st.st_mode, S_IFDIR | 0777);
  ck_assert_int_eq(st.st_nlink, 1);
}
END_TEST

START_TEST(test_getattr_level) {
  struct stat st;
  ck_assert_int_eq(bsfs_getattr(tmp_fs, "/getattrlvl/", &st), 0);
  ck_assert_uint_eq(st.st_mode, S_IFDIR | 0777);
  ck_assert_int_eq(st.st_nlink, 1);
}
END_TEST

START_TEST(test_getattr_level_noent) {
  struct stat st;
  ck_assert_int_eq(bsfs_getattr(tmp_fs, "/ajksldfjaskldjfkljaskjf/", &st),
                   -ENOENT);
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

stego_key_t readdir_key;

static void readdir_fs_setup(void) {
  int fd = create_tmp_file(FS_DISK_SIZE);
  ck_assert_int_eq(bsfs_init(fd, &tmp_fs), 0);

  ck_assert_int_eq(stego_gen_user_keys(&readdir_key, 1), 0);
  ck_assert_int_eq(keytab_store(tmp_fs->disk, 0, "readdirlvl", &readdir_key),
                   0);

  void* zero = calloc(1, BFT_SIZE);
  ck_assert(zero);
  ck_assert_int_eq(bft_write_table(&readdir_key, tmp_fs->disk, zero), 0);
  ck_assert_int_eq(fs_write_bitmap(&readdir_key, tmp_fs->disk, zero), 0);
  free(zero);

  ck_assert_int_eq(
      bsfs_mknod(tmp_fs, "/readdirlvl/file1", S_IFREG | S_IRUSR | S_IWUSR), 0);
  ck_assert_int_eq(bsfs_mknod(tmp_fs, "/readdirlvl/file2", S_IFREG), 0);
  ck_assert_int_eq(bsfs_mknod(tmp_fs, "/readdirlvl/file3",
                              S_IFREG | S_IRGRP | S_IROTH | S_IRUSR | S_IWUSR),
                   0);
  ck_assert_int_eq(bsfs_mknod(tmp_fs, "/readdirlvl/file4", S_IFREG | S_IWUSR),
                   0);
  ck_assert_int_eq(bsfs_mknod(tmp_fs, "/readdirlvl/file5", S_IFREG | S_IROTH),
                   0);
}

static void readdir_fs_teardown(void) {
  bsfs_destroy(tmp_fs);
}

struct test_readdir_ctx {
  int files;
};

static bool test_readdir_iter(const char* name, const struct stat* st,
                              void* raw_ctx) {
  struct test_readdir_ctx* ctx = (struct test_readdir_ctx*) raw_ctx;
  if (!strcmp(name, ".")) {
    ctx->files++;
  } else if (!strcmp(name, "..")) {
    ctx->files++;
  } else if (!strcmp(name, "file1")) {
    ck_assert_uint_eq(st->st_mode, S_IFREG | S_IRUSR | S_IWUSR);
    ctx->files++;
  } else if (!strcmp(name, "file2")) {
    ck_assert_uint_eq(st->st_mode, S_IFREG);
    ctx->files++;
  } else if (!strcmp(name, "file3")) {
    ck_assert_uint_eq(st->st_mode,
                      S_IFREG | S_IRGRP | S_IROTH | S_IRUSR | S_IWUSR);
    ctx->files++;
  } else if (!strcmp(name, "file4")) {
    ck_assert_uint_eq(st->st_mode, S_IFREG | S_IWUSR);
    ctx->files++;
  } else if (!strcmp(name, "file5")) {
    ck_assert_uint_eq(st->st_mode, S_IFREG | S_IROTH);
    ctx->files++;
  } else {
    ck_abort_msg("unexpected file");
  }

  return true;
}

START_TEST(test_readdir) {
  struct test_readdir_ctx ctx = { 0 };

  ck_assert_int_eq(bsfs_readdir(tmp_fs, "readdirlvl", test_readdir_iter, &ctx),
                   0);
  ck_assert_int_eq(ctx.files, 7);
}
END_TEST

START_TEST(test_readdir_nonexistent) {
  ck_assert_int_eq(bsfs_readdir(tmp_fs, "asfasfojashfjahfgkjasgfioagsvfuhl",
                                test_readdir_iter, NULL),
                   -ENOENT);
}
END_TEST

START_TEST(test_readdir_with_file) {
  ck_assert_int_eq(
      bsfs_readdir(tmp_fs, "readdirlvl/file1", test_readdir_iter, NULL),
      -ENOTDIR);
}
END_TEST

static bool test_readdir_bailout_iter(const char* name, const struct stat* st,
                                      void* raw_ctx) {
  (void) name;
  (void) st;

  struct test_readdir_ctx* ctx = (struct test_readdir_ctx*) raw_ctx;
  return ++ctx->files < 4;
}

START_TEST(test_readdir_iter_bailout) {
  struct test_readdir_ctx ctx = { 0 };
  ck_assert_int_eq(
      bsfs_readdir(tmp_fs, "readdirlvl", test_readdir_bailout_iter, &ctx), 0);
  ck_assert_int_eq(ctx.files, 4);
}
END_TEST

START_TEST(test_readdir_root) {
  struct test_readdir_ctx ctx = { 0 };
  ck_assert_int_eq(bsfs_readdir(tmp_fs, "/", test_readdir_iter, &ctx), 0);
  ck_assert_int_eq(ctx.files, 2);
}
END_TEST

stego_key_t rw_key;
bs_open_level_t rw_level;
cluster_offset_t rw_extend_cluster;

static void rw_fs_setup(void) {
  int fd = create_tmp_file(FS_DISK_SIZE + 0x4000);
  ck_assert_int_eq(bsfs_init(fd, &tmp_fs), 0);

  ck_assert_int_eq(stego_gen_user_keys(&rw_key, 1), 0);
  ck_assert_int_eq(keytab_store(tmp_fs->disk, 0, "readwritelvl", &rw_key), 0);

  void* zero = calloc(1, BFT_SIZE);
  ck_assert(zero);
  ck_assert_int_eq(bft_write_table(&rw_key, tmp_fs->disk, zero), 0);
  ck_assert_int_eq(fs_write_bitmap(&rw_key, tmp_fs->disk, zero), 0);
  free(zero);

  ck_assert_int_eq(bsfs_mknod(tmp_fs, "readwritelvl/file", S_IFREG), 0);

  ck_assert_int_eq(bs_level_get(tmp_fs, "readwritelvl", &rw_level), 0);
  ck_assert_int_eq(
      fs_alloc_cluster(rw_level->bitmap,
                       fs_count_clusters(stego_compute_user_level_size(
                           disk_get_size(rw_level->fs->disk))),
                       &rw_extend_cluster),
      0);
}

static void rw_fs_teardown(void) {
  bsfs_destroy(tmp_fs);
}

static const char* long_data =
    "asfjkAHGLFuikjwshgAPIGHSipughsiadpohguisiufghbnsipTHGPywiurtgyh "
    "8sEGHFihsayioufgtyseuifghaifHJIOShtrjizsdthojasytfgu tyy8eywtiu "
    "asgihhasf vnsfoiu vyhljwsgnhvij asytgjhsnrepoui tyojsknvigj "
    "yhatiumnjdhgij hnfnvij ehjrgnaiuertyhoiahbngonwlgajrhiengkj5qgfjlra "
    "ahaintgopTERHTUIwngfopuhe4rgniurebgvjiSHbroifbjOASIgrhjoksANPIOUJDNVPOIQ"
    "AFEMNBVPOIWHFKGNROUGNVFOi "
    "aHRLJKDFNBKULJGVBNMPKSFLVHJKLSJFLANMPKLRGNDFJNZOGJBNPOkprfngoisndgpibhsi"
    "gnapouj b iposjoudngpkZMv [JOUH FIURHIJ dksfog jsoktjgozpjdyrio djyhpz "
    "kdhkjiozj ghoixjmthpdjkghipsjktrmo9ipbrk;lnmlkstf "
    "jmhj;ktrjm;hkljdfg;h,s;dfvfd;klhj;krwsjkfhj;sgjmfldkymkpfjh;"
    "dslgmnasedmtosidbvnsp "
    "gspighs;ljt[poj4w;,sjb;'lsjmfb[lsdm,r[pohg,[spoykpz.,gp[sr,myho["
    "sdhkbwsel[gkm,[sorjhpszojhbl'sfmyhopsrjb[d'f,h[oxjmfbn'ld,hpksdfjsf["
    "oymoplgnj kldfmh;l "
    "jfhrl',ym';ltjhk;ldry,jkm;fhjkasjmetklshtklngzjldghldawern "
    "/,lmsahffjlkhasljfhaskljfaljsfhbjkoadfyghm,."
    "asdkljghajhflkahflkahflkayhklfhakfhaklserfhaklfjl;"
    "kahEFJLKNEWRKJFGNWSJLKBNFGJDZANFGLKANFKLANFKLNAKLRNQAJLFB,MSA DBG "
    "WESNBTG"
    "asfjkAHGLFuikjwshgAPIASKJFGHAJFHGLAHFGOIAEYHTGNLJKAEBNGLMSABDKIJSHRWNGJK"
    "LBHNSDJIGBSJIKDBGVHSEDMN "
    "KGBJNWSGKJMSNDKJGHSDJKGBKJZDBGKJASDGJKZDABHVKVBDKJLBNVKJSDNBFGLKLZDNGLKD"
    "SZHVBESKLGHLESHNTLJSHRKALYHFGKLASEHTRNLKASDFHGHSipughsiadpohguisiufghbns"
    "ipTHGPywiurtgyh 8sEGHFihsayioufgtyseuifghaifHJIOShtrjizsdthojasytfgu "
    "tyy8eywtiu asgihhasf vnsfoiu vyhljwsgnhvij asytgjhsnrepoui tyojsknvigj "
    "yhatiumnjdhgij hnfnvij ehjrgnaiuertyhoiahbngonwlgajrhiengkj5qgfjlra "
    "ahaintgopTERHTUIwngfopuhe4rgniurebgvjiSHbroifbjOASIgrhjoksANPIOUJDNVPOIQ"
    "AFEMNBVPOIWHFKGNROUGNVFOi "
    "aHRLJKDFNBKULJGVBNMPKSFLVHJKLSJFLANMPKLRGNDFJNZOGJBNPOkprfngoisndgpibhsi"
    "gnapouj b iposjoudngpkZMv [JOUH FIURHIJ dksfog jsoktjgozpjdyrio djyhpz "
    "kdhkjiozj ghoixjmthpdjkghipsjktrmo9ipbrk;lnmlkstf "
    "jmhj;ktrjm;hkljdfg;h,s;dfvfd;klhj;krwsjkfhj;sgjmfldkymkpfjh;"
    "dslgmnasedmtosidbvnsp "
    "gspighs;ljt[poj4w;,sjb;'lsjmfb[lsdm,r[pohg,[spoykpz.,gp[sr,myho["
    "sdhkbwsel[gkm,[sorjhpszojhbl'sfmyhopsrjb[d'f,h[oxjmfbn'ld,hpksdfjsf["
    "oymoplgnj kldfmh;l "
    "jfhrl',ym';ltjhk;ldry,jkm;fhjkasjmetklshtklngzjldghldawern "
    "/,lmsahffjlkhasljfhaskljfaljsfhbjkoadfyghm,."
    "asdkljghajhflkahflkahflkayhklfhakfhaklserfhaklfjl;"
    "kahEFJLKNEWRKJFGNWSJLKBNFGJDZANFGLKANFKLANFKLNAKLRNQAJLFB,MSA DBG "
    "WESNBTGJ";

static const char* long_data2 =
    "Lorem ipsum dolor sit amet, mea cu consul moderatius, et eum prima nostro "
    "petentium. Ea quo sint putant. Numquam sensibus ut nec, vix dicat "
    "iracundia liberavisse ad, quot discere fuisset in qui. Ne vix veri "
    "maiorum interpretaris, ne viris facete fastidii his. Sed in fuisset "
    "appellantur consequuntur. In quo etiam aliquando conceptam. Ius et quidam "
    "legendos, an eum rebum labitur, id debitis habemus platonem nam."

    "Prompta sadipscing qui no,"
    "eam ei possim nostrum euripidis.Te est nisl putant principes,"
    "soleat dissentiunt eam eu,"
    "wisi tacimates nec ei.Ius"
    "populo facete et.Choro regione recteque ad"
    "has.Ei sit menandri consetetur liberavisse"

    "Eum nisl utinam mucius ex.Et tollit doctus audiam vix.Choro"
    "oporteat ne sed,"
    "an mucius semper labores quo.Pri cu dicit debitis"

    "Vix mentitum offendit contentiones ei.An"
    "mea aperiri accumsan.Clita partiendo eloquentiam ad mea,"
    "tantas feugiat sit eu.Per lorem persecuti no,"
    "te pro purto bonorum omittantur,"
    "iusto partem et has.Vim tota saepe virtute cu."

    "Ei eam commune signiferumque,"
    "ea eam ferri singulis reprehendunt.Ut"
    "habeo mutat lucilius vis.Pri at placerat repudiandae.Dicam"
    "periculis abhorreant quo no,"
    "sit dolor utamur delectus te,"
    "vix aeque volutpat an.Id elitr percipit apeirian eum,"
    "eos simul nominavi ea.His solum reprehendunt in,"
    "duo no vocibus nostrum.Pro euripidis efficiendi et,"
    "veniam dolorem vivendum est no,"
    "ponderum explicari theophrastus id"
    "sea."

    "Wisi philosophia vim an.Malorum vivendo copiosae ad vis,"
    "ut veniam legere eos,"
    "eu vix populo integre percipit.Principes definitionem in mei,"
    "vix reque diceret at,"
    "tota salutatus quo te.Elitr omnes sea no,"
    "est omnes evertitur ut.Et ius nusquam accusam blandit,"
    "at qui probo ponderum signiferumque,"
    "tollit mentitum probatus ne mea."

    "Id graece mollis aliquid vim,"
    "graeci doming molestiae eu pro.Eos eu quem mollis,"
    "his mutat quando altera id.Ne sit dicant antiopam moderatius,"
    "id quo consulatu dissentiunt definitionem"
    ".Vel tamquam molestie an.Nec eu elitr primis albucius,"
    "soluta electram ei vix, doming audiam nam id.Porro interpretaris ad vel,";
// cum in vero wisi erroribus,
// ubique vocent scriptorem id pro.

// Et eam dico causae pertinax,
// ei porro mazim duo.Mel ea aperiam labores recusabo,
// alterum principes incorrupte nec ne.Duo at fabellas repudiare,
// ne vis iisque facilisi consequat.Qui ne aliquam referrentur,
// ancillae prodesset et pro.Has brute accusam signiferumque no,
// solet altera phaedrum duo id.Te his modo munere.

// Duo denique appetere in,
// vim te agam nihil.Unum laoreet deleniti at eam,
// ad vim iudico theophrastus.Zril oporteat adversarium mel ex,
// omnes dolores vis te.Ex tempor dolores consulatu vel,
// numquam tacimates id pro.Vim sonet deseruisse at.An
//     nemore feugiat temporibus mei,
// vidit dolor option pro
//     te.

// Quas iracundia contentiones ei sit.Mel dolorum intellegam assueverit et,
// no facete honestatis qui,
// ex has dico scribentur.Vocibus democritum signiferumque ea vim,
// sanctus delectus expetenda te his.Altera percipit mea te,
// ut modus altera tibique sit.Ad quo erant utamur interesset.Sea
//     no malorum oportere prodesset,
// purto facer evertitur mei ad.

// At mei brute graecis forensibus,
// ei prompta saperet perpetua mei.Pri no quem.";
START_TEST(test_do_write_extend_within_cluster) {
  char buf[CLUSTER_SIZE] = "asjfhakjsfh";
  fs_set_next_cluster(buf, CLUSTER_OFFSET_EOF);
  ck_assert_int_eq(
      fs_write_cluster(&rw_key, rw_level->fs->disk, buf, rw_extend_cluster), 0);

  off_t local_eof_off = strlen(buf);

  const char* new_data =
      "\n\"No, I am your father\".\n \"WHAAAAAT\" - Said Luke,\n \"FUCK OFF!\"";
  ck_assert_int_eq(bs_do_write_extend(rw_level, rw_extend_cluster,
                                      local_eof_off, new_data, strlen(new_data),
                                      0),
                   0);
  char read[CLUSTER_SIZE];
  ck_assert_int_eq(
      fs_read_cluster(&rw_key, rw_level->fs->disk, read, rw_extend_cluster), 0);

  char expected[CLUSTER_SIZE] = { 0 };
  memcpy(expected, buf, local_eof_off);
  strcpy(expected + local_eof_off, new_data);
  fs_set_next_cluster(expected, CLUSTER_OFFSET_EOF);

  ck_assert_int_eq(memcmp(read, expected, CLUSTER_SIZE), 0);
}
END_TEST

START_TEST(test_do_write_extend_outside_of_cluster) {
  char buf[CLUSTER_SIZE] = "asjfhakjsfh";
  fs_set_next_cluster(buf, CLUSTER_OFFSET_EOF);
  ck_assert_int_eq(
      fs_write_cluster(&rw_key, rw_level->fs->disk, buf, rw_extend_cluster), 0);

  off_t local_eof_off = strlen(buf);

  ck_assert_int_eq(bs_do_write_extend(rw_level, rw_extend_cluster,
                                      local_eof_off, long_data,
                                      strlen(long_data), 0),
                   0);
  char read_total[2 * CLUSTER_DATA_SIZE] = { 0 };

  char read[CLUSTER_SIZE];
  ck_assert_int_eq(
      fs_read_cluster(&rw_key, rw_level->fs->disk, read, rw_extend_cluster), 0);

  memcpy(read_total, read, CLUSTER_DATA_SIZE);

  ck_assert_int_eq(
      fs_read_cluster(&rw_key, rw_level->fs->disk, read, fs_next_cluster(read)),
      0);

  memcpy(read_total + CLUSTER_DATA_SIZE, read, CLUSTER_DATA_SIZE);

  char expected[2 * CLUSTER_DATA_SIZE] = { 0 };
  memcpy(expected, buf, local_eof_off);
  strcpy(expected + local_eof_off, long_data);

  ck_assert_int_eq(
      memcmp(read_total, expected, local_eof_off + strlen(long_data)), 0);
}
END_TEST

START_TEST(test_do_write_extend_off_outside_of_cluster) {
  char buf[CLUSTER_SIZE] = "asjfhakjsfh";
  fs_set_next_cluster(buf, CLUSTER_OFFSET_EOF);
  ck_assert_int_eq(
      fs_write_cluster(&rw_key, rw_level->fs->disk, buf, rw_extend_cluster), 0);

  off_t local_eof_off = strlen(buf);

  const char* new_data = "The Corridor by Iddo Shavit"
                         "The door was open"
                         "And the lock,"
                         "broken So I walk right in In my hand,"
                         "a token And it's not always dark"
                         "And on the wall, a mark."

                         "It goes on forever But at the end,"
                         "a lever And once pulled, it will sever"
                         "It carves a hole in the wall"

                         "And the wedge is small,"
                         "Token-sized"
                         "So in goes the token Never to be prized"

                         "And when I return"
                         "The mark is gone"
                         "And will in-turn"
                         "To ashes burn"

                         "And I walk back out of the hall"
                         "Into eternal freedom"
                         "But the door isn't open"
                         "And the lock, not broken.";
  off_t off = CLUSTER_SIZE + 300;
  ck_assert_int_eq(bs_do_write_extend(rw_level, rw_extend_cluster,
                                      local_eof_off, new_data, strlen(new_data),
                                      off),
                   0);
  char read_total[2 * CLUSTER_DATA_SIZE] = { 0 };

  char read[CLUSTER_SIZE];
  ck_assert_int_eq(
      fs_read_cluster(&rw_key, rw_level->fs->disk, read, rw_extend_cluster), 0);

  memcpy(read_total, read, CLUSTER_DATA_SIZE);

  ck_assert_int_eq(
      fs_read_cluster(&rw_key, rw_level->fs->disk, read, fs_next_cluster(read)),
      0);

  memcpy(read_total + CLUSTER_DATA_SIZE, read, CLUSTER_DATA_SIZE);

  char expected[2 * CLUSTER_DATA_SIZE] = { 0 };
  memcpy(expected, buf, local_eof_off);
  strcpy(expected + local_eof_off + off, new_data);

  ck_assert_int_eq(
      memcmp(read_total, expected, local_eof_off + off + strlen(new_data)), 0);
}
END_TEST

START_TEST(test_do_write_extend_off_buf_outside_of_cluster) {
  char buf[CLUSTER_SIZE] = "asjfhakjsfh";
  fs_set_next_cluster(buf, CLUSTER_OFFSET_EOF);
  ck_assert_int_eq(
      fs_write_cluster(&rw_key, rw_level->fs->disk, buf, rw_extend_cluster), 0);

  off_t local_eof_off = strlen(buf);

  off_t off = CLUSTER_SIZE + 300;
  ck_assert_int_eq(bs_do_write_extend(rw_level, rw_extend_cluster,
                                      local_eof_off, long_data,
                                      strlen(long_data), off),
                   0);
  char read_total[3 * CLUSTER_DATA_SIZE] = { 0 };

  char read[CLUSTER_SIZE];
  ck_assert_int_eq(
      fs_read_cluster(&rw_key, rw_level->fs->disk, read, rw_extend_cluster), 0);

  memcpy(read_total, read, CLUSTER_DATA_SIZE);

  ck_assert_int_eq(
      fs_read_cluster(&rw_key, rw_level->fs->disk, read, fs_next_cluster(read)),
      0);

  memcpy(read_total + CLUSTER_DATA_SIZE, read, CLUSTER_DATA_SIZE);

  ck_assert_int_eq(
      fs_read_cluster(&rw_key, rw_level->fs->disk, read, fs_next_cluster(read)),
      0);

  memcpy(read_total + 2 * CLUSTER_DATA_SIZE, read, CLUSTER_DATA_SIZE);

  char expected[3 * CLUSTER_DATA_SIZE] = { 0 };
  memcpy(expected, buf, local_eof_off);
  strcpy(expected + local_eof_off + off, long_data);

  ck_assert_int_eq(
      memcmp(read_total, expected, local_eof_off + off + strlen(long_data)), 0);
}
END_TEST

START_TEST(test_do_write_extend_cluster_full) {
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

  TCase* path_tcase = tcase_create("path");
  tcase_add_test(path_tcase, test_split_path);
  tcase_add_test(path_tcase, test_split_path_no_slash);
  tcase_add_test(path_tcase, test_split_path_double_slash);
  tcase_add_test(path_tcase, test_get_dirname);
  tcase_add_test(path_tcase, test_get_dirname_trailing_slash);
  tcase_add_test(path_tcase, test_get_dirname_with_file);
  suite_add_tcase(suite, path_tcase);

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
  tcase_add_test(getattr_tcase, test_getattr_root);
  tcase_add_test(getattr_tcase, test_getattr_level);
  tcase_add_test(getattr_tcase, test_getattr_level_noent);
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

  TCase* readdir_tcase = tcase_create("readdir");
  tcase_add_checked_fixture(readdir_tcase, readdir_fs_setup,
                            readdir_fs_teardown);
  tcase_add_test(readdir_tcase, test_readdir);
  tcase_add_test(readdir_tcase, test_readdir_nonexistent);
  tcase_add_test(readdir_tcase, test_readdir_with_file);
  tcase_add_test(readdir_tcase, test_readdir_iter_bailout);
  tcase_add_test(readdir_tcase, test_readdir_root);
  suite_add_tcase(suite, readdir_tcase);

  TCase* read_write_tcase = tcase_create("read_write");
  tcase_add_checked_fixture(read_write_tcase, rw_fs_setup, rw_fs_teardown);
  tcase_add_test(read_write_tcase, test_do_write_extend_within_cluster);
  tcase_add_test(read_write_tcase, test_do_write_extend_outside_of_cluster);
  tcase_add_test(read_write_tcase, test_do_write_extend_off_outside_of_cluster);
  tcase_add_test(read_write_tcase,
                 test_do_write_extend_off_buf_outside_of_cluster);
  tcase_add_test(read_write_tcase, test_do_write_extend_cluster_full);
  suite_add_tcase(suite, read_write_tcase);

  return suite;
}
