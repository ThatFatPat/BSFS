#ifndef BS_BSFS_H
#define BS_BSFS_H

#include <stdbool.h>
#include <stddef.h>
#include <sys/stat.h>
#include <sys/types.h>

#ifndef RENAME_NOREPLACE
#define RENAME_NOREPLACE (1 << 0) /* Don't overwrite target */
#endif
#ifndef RENAME_EXCHANGE
#define RENAME_EXCHANGE (1 << 1) /* Exchange source and dest */
#endif
#ifndef RENAME_WHITEOUT
#define RENAME_WHITEOUT (1 << 2)
#endif

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
int bsfs_fgetattr(bs_file_t file, struct stat* st);

int bsfs_chmod(bs_bsfs_t fs, const char* path, mode_t mode);
int bsfs_fchmod(bs_file_t file, mode_t mode);

int bsfs_truncate(bs_bsfs_t fs, const char* path, off_t size);
int bsfs_ftruncate(bs_file_t file, off_t size);

int bsfs_utimens(bs_bsfs_t fs, const char* path,
                 const struct timespec times[2]);
int bsfs_futimens(bs_file_t file, const struct timespec times[2]);

int bsfs_rename(bs_bsfs_t fs, const char* src_path, const char* new_name,
                unsigned int flags);

typedef int (*bs_dir_iter_t)(const char* name, const struct stat* st,
                             void* ctx);
int bsfs_readdir(bs_bsfs_t fs, const char* name, bs_dir_iter_t iter, void* ctx);

#endif
