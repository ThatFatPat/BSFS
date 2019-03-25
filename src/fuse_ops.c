#include "fuse_ops.h"

#include "bsfs.h"
#include <stdbool.h>
#include <unistd.h>

#define LONG_TIMEOUT 500000 // Arbitrarily long timeout

int bsfs_fuse_init(struct fuse_conn_info* conn, struct fuse_config* cfg) {
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

  return 0;
}

void bsfs_fuse_destroy(void* private_data) {
  bsfs_destroy((bs_bsfs_t) private_data);
}

int bsfs_fuse_mknod(const char* path, mode_t mode, dev_t dev) {
}

int bsfs_fuse_unlink(const char* path) {
}

int bsfs_fuse_open(const char* path, struct fuse_file_info* fi) {
}

int bsfs_fuse_release(const char* path, struct fuse_file_info* fi) {
}

int bsfs_fuse_read(const char* path, char* buf, size_t size, off_t off,
                   struct fuse_file_info* fi) {
}

int bsfs_fuse_write(const char* path, const char* buf, size_t size, off_t off,
                    struct fuse_file_info* fi) {
}

int bsfs_fuse_fsync(const char* path, int datasync, struct fuse_file_info* fi) {
}

int bsfs_fuse_getattr(const char* path, struct stat* st,
                      struct fuse_file_info* fi) {
}

int bsfs_fuse_chmod(const char* path, mode_t mode, struct fuse_file_info* fi) {
}

int bsfs_fuse_truncate(const char* path, off_t size,
                       struct fuse_file_info* fi) {
}

int bsfs_fuse_utimens(const char* path, const struct timespec times[2],
                      struct fuse_file_info* fi) {
}

int bsfs_fuse_rename(const char* oldpath, const char* newpath,
                     unsigned int flags) {
}
