#ifndef BS_KEYTAB_H
#define BS_KEYTAB_H

#include "disk.h"
#include <sys/types.h>

#define KEYTAB_MAX_LEVELS 16
#define KEYTAB_ENTRY_SIZE 32

int keytab_lookup(bs_disk_t disk, const char* password, void* key);
int keytab_store(bs_disk_t disk, off_t index, const char* password,
                 const void* key);

#endif // BS_KEYTAB_H
