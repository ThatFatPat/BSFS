#ifndef BS_KEYTAB_H
#define BS_KEYTAB_H
#include "disk.h"
#include "enc.h"
#include <sys/types.h>

#define KEYTAB_MAGIC 0xBEEFCAFE
#define KEYTAB_MAGIC_SIZE (sizeof KEYTAB_MAGIC)
#define MAX_LEVELS 16
#define KEYTAB_KEY_SIZE 1
#define KEYTAB_ENTRY_SIZE 32

int keytab_lookup(bs_disk_t disk, const void* password, void* key);
int keytab_store(bs_disk_t disk, off_t index, const char* password,
                 const void* key);

#endif // BS_KEYTAB_H
