#include "mkbsfs.h"

#include "bsfs.h"
#include "stego.h"
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/**
 * Trim a string
 */
static int trim(const char* str, char** out) {
  while (isspace(*str)) {
    str++;
  }

  size_t len = strlen(str);
  while (len && isspace(str[len - 1])) {
    len--;
  }

  errno = 0;
  *out = strndup(str, len);
  return -errno;
}

/**
 * Extract the passwords from the passwords file into an array
 */
static int get_passwords(const char* passfile_path, char** passwords,
                         size_t* out_count) {
  FILE* passfile = fopen(passfile_path, "r");
  if (!passfile) {
    return -errno;
  }

  int ret = 0;

  size_t count = 0;
  char* line = NULL;
  size_t line_len = 0;

  while (getline(&line, &line_len, passfile) >= 0) {
    if (count >= STEGO_USER_LEVEL_COUNT - 1) {
      ret = MKBSFS_TOO_MANY_PASSWORDS;
      goto fail;
    }

    ret = trim(line, &passwords[count++]);
    if (ret < 0) {
      goto fail;
    }
  }

  *out_count = count;
  goto cleanup;

fail:
  for (size_t i = 0; i < count; i++) {
    free(passwords[i]);
  }

cleanup:
  free(line);
  fclose(passfile);
  return ret;
}

int mkbsfs(const char* disk_path, const char* passfile_path) {
  int disk_fd = open(disk_path, O_RDWR);
  if (disk_fd < 0) {
    return -errno;
  }

  char* passwords[STEGO_USER_LEVEL_COUNT];
  size_t levels;
  int ret = get_passwords(passfile_path, passwords, &levels);
  if (ret < 0 || ret == MKBSFS_TOO_MANY_PASSWORDS) {
    goto cleanup_disk;
  }

  ret = bsfs_format(disk_fd, levels, (const char* const*) passwords);

  for (size_t i = 0; i < levels; i++) {
    free(passwords[i]);
  }

cleanup_disk:
  close(disk_fd);
  return ret;
}
