#ifndef BS_KEYTAB_H
#define BS_KEYTAB_H

#include "disk.h"
#include "enc.h"
#include <sys/types.h>

#define KEYTAB_MAGIC 0xBEEFCAFE
#define MAX_LEVELS 16

int keytab_lookup(bs_disk_t disk, const void* password, void* key);
int keytab_store(bs_disk_t disk, off_t index, const char* password,
                 const void* key);

#endif // BS_KEYTAB_H
