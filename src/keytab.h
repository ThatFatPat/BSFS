#ifndef BS_KEYTAB_H
#define BS_KEYTAB_H
#include "disk.h"
#include <sys/types.h>

#define KEYTAB_ENTRY_SIZE 32
#define MAX_LEVELS 16

int keytab_lookup(bs_disk_t disk, const char* pass, void* key);
int keytab_store(bs_disk_t disk, off_t index, const char* pass,
                 const void* key);
#endif // BS_KEYTAB_H
