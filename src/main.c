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

int main(int argc, char* argv[]) {
  struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
  struct bsfs_config config = { 0 };

  if (fuse_opt_parse(&args, &config, bsfs_opts, bsfs_opt_proc) < 0) {
    fprintf(stderr, "error: failed to parse arguments\n");
    return 1;
  }

  return 0;
}
