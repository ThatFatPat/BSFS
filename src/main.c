#include "bft.h"
#include "bsfs.h"
#include "disk.h"
#include "fuse_ops.h"
#include "fuse_wrap.h"
#include "keytab.h"
#include "stego.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static const struct fuse_operations ops = { .init = bsfs_fuse_init,
                                            .destroy = bsfs_fuse_destroy,
                                            .mknod = bsfs_fuse_mknod,
                                            .unlink = bsfs_fuse_unlink,
                                            .open = bsfs_fuse_open,
                                            .release = bsfs_fuse_release,
                                            .opendir = bsfs_fuse_opendir,
                                            .read = bsfs_fuse_read,
                                            .write = bsfs_fuse_write,
                                            .fsync = bsfs_fuse_fsync,
                                            .getattr = bsfs_fuse_getattr,
                                            .chmod = bsfs_fuse_chmod,
                                            .truncate = bsfs_fuse_truncate,
                                            .utimens = bsfs_fuse_utimens,
                                            .rename = bsfs_fuse_rename,
                                            .readdir = bsfs_fuse_readdir };

int create_file() {
  int fd =
      open("/home/noam/bsfstest", O_RDWR, O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
  if (fd < 0) {
    return -errno;
  }

  int ret = ftruncate(fd, KEYTAB_SIZE +
                              STEGO_USER_LEVEL_COUNT * (BFT_SIZE + 0x10000));
  if (ret < 0) {
    return ret;
  }

  return fd;
}

int format_bsfs(int fd) {
  bs_disk_t disk;
  int ret = disk_create(fd, &disk);
  if (ret < 0) {
    return ret;
  }

  stego_key_t keys[2];
  const char* pass1 = "Noam";
  const char* pass2 = "Iddo";
  ret = stego_gen_user_keys(keys, 2);
  if (ret < 0) {
    return ret;
  }

  ret = keytab_store(disk, 0, pass1, keys);
  if (ret < 0) {
    return ret;
  }

  ret = keytab_store(disk, 1, pass2, keys + 1);
  if (ret < 0) {
    return ret;
  }

  size_t bitmap_size = fs_compute_bitmap_size_from_disk(disk);
  void* zero = calloc(1, BFT_SIZE + bitmap_size);

  for (size_t i = 0; i < 2; i++) {
    ret = bft_write_table(keys + i, disk, zero);
    if (ret < 0) {
      return ret;
    }

    ret = fs_write_bitmap(keys + i, disk, zero);
    if (ret < 0) {
      return ret;
    }
  }

  free(zero);

  return ret;
}

int mount_bsfs(int argc, char* argv[], int fd) {
  bs_bsfs_t fs;
  int ret = bsfs_init(fd, &fs);
  if (ret < 0) {
    perror("Setup failed");
    exit(1);
  }

  return fuse_main(argc, argv, &ops, fs);
}

int main(int argc, char* argv[]) {
  int fd = create_file();
  if (fd < 0) {
    printf("Create failed: %s", strerror(-fd));
    return 1;
  }
  int ret = format_bsfs(fd);
  if (ret < 0) {
    printf("Format failed: %s", strerror(-ret));
    return 1;
  }
  return mount_bsfs(argc, argv, fd);
}
