#include "bsfs.h"
#include "fuse_ops.h"
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct bsfs_config {
  const char* disk_path;
  int show_help;
  int show_version;
};

#define BSFS_OPT(templ, memb)                                                  \
  { templ, offsetof(struct bsfs_config, memb), 1 }

static int bsfs_opt_proc(void* data, const char* arg, int key,
                         struct fuse_args* args) {
  (void) args;
  struct bsfs_config* config = (struct bsfs_config*) data;
  if (key == FUSE_OPT_KEY_NONOPT && !config->disk_path) {
    config->disk_path = strdup(arg);
    if (!config->disk_path) {
      exit(1);
    }
    return 0;
  }
  return 1;
}

static void show_bsfs_help(const char* progname) {
  printf("usage: %s [options] <disk path> <mountpoint>\n\n", progname);
}

static int init_fs(bs_bsfs_t* fs, const char* disk_path) {
  int fd = open(disk_path, O_RDWR);
  if (fd < 0) {
    return -errno;
  }
  int ret = bsfs_init(fd, fs);
  if (ret < 0) {
    close(fd);
  }
  return ret;
}

static const struct fuse_opt bsfs_opts[] = {
  BSFS_OPT("-h", show_help), BSFS_OPT("--help", show_help),
  BSFS_OPT("-V", show_version), BSFS_OPT("--version", show_version),
  FUSE_OPT_END
};

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
                                            .fallocate = bsfs_fuse_fallocate,
                                            .rename = bsfs_fuse_rename,
                                            .readdir = bsfs_fuse_readdir };

int main(int argc, char* argv[]) {
  struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
  struct bsfs_config config = { 0 };

  if (fuse_opt_parse(&args, &config, bsfs_opts, bsfs_opt_proc) < 0) {
    return 1;
  }

  if (config.show_version) {
    // TODO: show bsfs version?
    if (fuse_opt_add_arg(&args, "-V") < 0) {
      return 1;
    }
    return fuse_main(args.argc, args.argv, &ops, NULL);
  }

  if (config.show_help) {
    show_bsfs_help(args.argv[0]);
    if (fuse_opt_add_arg(&args, "-h") < 0) {
      return 1;
    }
    args.argv[0][0] = '\0'; // Prevent FUSE from printing usage.
    return fuse_main(args.argc, args.argv, &ops, NULL);
  }

  if (!config.disk_path) {
    fprintf(stderr, "error: no disk path specified\n");
    return 1;
  }

  if (fuse_opt_add_arg(&args, "-odefault_permissions") < 0) {
    return 1;
  }

  bs_bsfs_t fs;
  int ret = init_fs(&fs, config.disk_path);
  if (ret < 0) {
    fprintf(stderr, "error: initialization failed: %s\n", strerror(-ret));
    return 1;
  }

  return fuse_main(args.argc, args.argv, &ops, fs);
}
