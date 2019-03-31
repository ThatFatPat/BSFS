#ifndef BS_FUSE_OPS_H
#define BS_FUSE_OPS_H

#include "fuse_wrap.h"

void* bsfs_fuse_init(struct fuse_conn_info* conn, struct fuse_config* cfg);
void bsfs_fuse_destroy(void* private_data);

int bsfs_fuse_mknod(const char* path, mode_t mode, dev_t dev);
int bsfs_fuse_unlink(const char* path);

int bsfs_fuse_open(const char* path, struct fuse_file_info* fi);
int bsfs_fuse_release(const char* path, struct fuse_file_info* fi);

int bsfs_fuse_opendir(const char* path, struct fuse_file_info* fi);

int bsfs_fuse_read(const char* path, char* buf, size_t size, off_t off,
                   struct fuse_file_info* fi);
int bsfs_fuse_write(const char* path, const char* buf, size_t size, off_t off,
                    struct fuse_file_info* fi);

int bsfs_fuse_fsync(const char* path, int datasync, struct fuse_file_info* fi);

int bsfs_fuse_getattr(const char* path, struct stat* st,
                      struct fuse_file_info* fi);

int bsfs_fuse_chmod(const char* path, mode_t mode, struct fuse_file_info* fi);
int bsfs_fuse_truncate(const char* path, off_t size, struct fuse_file_info* fi);
int bsfs_fuse_utimens(const char* path, const struct timespec times[2],
                      struct fuse_file_info* fi);

int bsfs_fuse_rename(const char* oldpath, const char* newpath,
                     unsigned int flags);

int bsfs_fuse_readdir(const char* path, void* buf, fuse_fill_dir_t filler,
                      off_t off, struct fuse_file_info* fi,
                      enum fuse_readdir_flags flags);

#endif // BS_FUSE_OPS_H
