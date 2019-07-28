#ifndef BS_MKBSFS_H
#define BS_MKBSFS_H

#define MKBSFS_TOO_MANY_PASSWORDS 0xdead
#define MKBSFS_INVALID_PASSWORD 0xbad

int mkbsfs(const char* disk_path, const char* passfile_path);

#endif // BS_MKBSFS_H
