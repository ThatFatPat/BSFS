#include "fuse_ops.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>

struct bsfs_config {
  const char* disk_path;
  int show_help;
};

#define BSFS_OPT(templ, memb)                                                  \
  { templ, offsetof(struct bsfs_config, memb), 1 }

const struct fuse_opt bsfs_opts[] = { BSFS_OPT("-h", show_help),
                                      BSFS_OPT("--help", show_help),
                                      FUSE_OPT_END };

static int bsfs_opt_proc(void* data, const char* arg, int key,
                         struct fuse_args* args) {
  (void) args;
  struct bsfs_config* config = (struct bsfs_config*) data;
  if (key == FUSE_OPT_KEY_NONOPT && !config->disk_path) {
    config->disk_path = strdup(arg);
    return 0;
  }
  return 1;
}

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
    fprintf(stderr, "error: failed to parse arguments\n");
    return 1;
  }

  return 0;
}
