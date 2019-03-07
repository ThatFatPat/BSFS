#ifndef BS_BSFS_H
#define BS_BSFS_H

#include <stdbool.h>
#include <stddef.h>
#include <sys/stat.h>
#include <sys/types.h>

typedef struct bs_file_impl* bs_file_t;
typedef struct bs_bsfs_impl* bs_bsfs_t;

int bsfs_init(int fd, bs_bsfs_t* out);
void bsfs_destroy(bs_bsfs_t fs);

int bsfs_mknod(bs_bsfs_t fs, const char* path, mode_t mode);
int bsfs_unlink(bs_bsfs_t fs, const char* path);

int bsfs_open(bs_bsfs_t fs, const char* path, bs_file_t* file);
int bsfs_release(bs_file_t file);

ssize_t bsfs_read(bs_file_t file, void* buf, size_t size, off_t off);
ssize_t bsfs_write(bs_file_t file, const void* buf, size_t size, off_t off);
int bsfs_fsync(bs_file_t file, bool datasync);

int bsfs_getattr(bs_bsfs_t fs, const char* path, struct stat* st);
int bsfs_setattr(bs_bsfs_t fs, const char* path, const struct stat* st);
int bsfs_rename(bs_bsfs_t fs, const char* src_path, const char* new_name,
                unsigned int flags);
int bsfs_chmod(bs_bsfs_t fs, const char* path, mode_t mode);
int bsfs_truncate(bs_bsfs_t fs, const char* path, off_t size);

typedef int (*bs_dir_iter_t)(const char* name, const struct stat* st,
                             void* ctx);
int bsfs_readdir(bs_bsfs_t fs, const char* name, bs_dir_iter_t iter, void* ctx);

#endif
