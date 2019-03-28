#include "fuse_ops.h"

#include "bsfs.h"
#include <stdbool.h>
#include <unistd.h>

#define LONG_TIMEOUT 500000 // Arbitrarily long timeout

static bs_bsfs_t get_fs(void) {
  return (bs_bsfs_t) fuse_get_context()->private_data;
}

static bs_file_t get_file(struct fuse_file_info* fi) {
  return (bs_file_t) fi->fh;
}

static bs_file_t try_get_file(struct fuse_file_info* fi) {
  return fi ? get_file(fi) : NULL;
}

static void set_file(struct fuse_file_info* fi, bs_file_t file) {
  fi->fh = (uint64_t) file;
}

void* bsfs_fuse_init(struct fuse_conn_info* conn, struct fuse_config* cfg) {
  conn->time_gran = 1000000000; // 1 second
  conn->want |= FUSE_CAP_WRITEBACK_CACHE;

  cfg->set_uid = true;
  cfg->uid = getuid();

  cfg->set_gid = true;
  cfg->gid = getgid();

  // The filesystem can't be changed externally when mounted, so let the kernel
  // cache everything.
  cfg->kernel_cache = true;
  cfg->attr_timeout = LONG_TIMEOUT;
  cfg->entry_timeout = LONG_TIMEOUT;

  return get_fs();
}

void bsfs_fuse_destroy(void* private_data) {
  bsfs_destroy((bs_bsfs_t) private_data);
}

int bsfs_fuse_mknod(const char* path, mode_t mode, dev_t dev) {
  (void) dev;
  return bsfs_mknod(get_fs(), path, mode);
}

int bsfs_fuse_unlink(const char* path) {
  return bsfs_unlink(get_fs(), path);
}

int bsfs_fuse_open(const char* path, struct fuse_file_info* fi) {
  bs_file_t file;
  int ret = bsfs_open(get_fs(), path, &file);
  if (ret < 0) {
    return ret;
  }
  set_file(fi, file);
  return 0;
}

int bsfs_fuse_release(const char* path, struct fuse_file_info* fi) {
  (void) path;
  return bsfs_release(get_file(fi));
}

int bsfs_fuse_opendir(const char* path, struct fuse_file_info* fi) {
  // All checks will be performed by the kernel - just make sure there's no
  // associated BSFS file so that other ops don't mistake it for a file.
  (void) path;
  set_file(fi, NULL);
  return 0;
}

int bsfs_fuse_read(const char* path, char* buf, size_t size, off_t off,
                   struct fuse_file_info* fi) {
  (void) path;
  return bsfs_read(get_file(fi), buf, size, off);
}

int bsfs_fuse_write(const char* path, const char* buf, size_t size, off_t off,
                    struct fuse_file_info* fi) {
  (void) path;
  return bsfs_write(get_file(fi), buf, size, off);
}

int bsfs_fuse_fsync(const char* path, int datasync, struct fuse_file_info* fi) {
  (void) path;
  return bsfs_fsync(get_file(fi), datasync);
}

int bsfs_fuse_getattr(const char* path, struct stat* st,
                      struct fuse_file_info* fi) {
  bs_file_t file = try_get_file(fi);
  return file ? bsfs_fgetattr(file, st) : bsfs_getattr(get_fs(), path, st);
}

int bsfs_fuse_chmod(const char* path, mode_t mode, struct fuse_file_info* fi) {
  bs_file_t file = try_get_file(fi);
  return file ? bsfs_fchmod(file, mode) : bsfs_chmod(get_fs(), path, mode);
}

int bsfs_fuse_truncate(const char* path, off_t size,
                       struct fuse_file_info* fi) {
  bs_file_t file = try_get_file(fi);
  return file ? bsfs_ftruncate(file, size)
              : bsfs_truncate(get_fs(), path, size);
}

int bsfs_fuse_utimens(const char* path, const struct timespec times[2],
                      struct fuse_file_info* fi) {
  bs_file_t file = try_get_file(fi);
  return file ? bsfs_futimens(file, times)
              : bsfs_utimens(get_fs(), path, times);
}

int bsfs_fuse_rename(const char* oldpath, const char* newpath,
                     unsigned int flags) {
  return bsfs_rename(get_fs(), oldpath, newpath, flags);
}

struct bsfs_fuse_readdir_ctx {
  fuse_fill_dir_t filler;
  void* buf;
  enum fuse_readdir_flags flags;
};

static bool bsfs_fuse_readdir_iter(const char* name, const struct stat* st,
                                   void* raw_ctx) {
  struct bsfs_fuse_readdir_ctx* ctx = (struct bsfs_fuse_readdir_ctx*) raw_ctx;

  // If the kernel has requested attributes ("plus" mode), and they are
  // available, let FUSE know that they are.
  enum fuse_fill_dir_flags filler_flags = 0;
  if (ctx->flags & FUSE_READDIR_PLUS && st) {
    filler_flags |= FUSE_FILL_DIR_PLUS;
  }

  return !ctx->filler(ctx->buf, name, st, 0, filler_flags);
}

int bsfs_fuse_readdir(const char* path, void* buf, fuse_fill_dir_t filler,
                      off_t off, struct fuse_file_info* fi,
                      enum fuse_readdir_flags flags) {
  (void) off;
  (void) fi;

  struct bsfs_fuse_readdir_ctx ctx = { .filler = filler,
                                       .buf = buf,
                                       .flags = flags };
  return bsfs_readdir(get_fs(), path, bsfs_fuse_readdir_iter, &ctx);
}
