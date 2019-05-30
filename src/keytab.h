#ifndef BS_KEYTAB_H
#define BS_KEYTAB_H

#include "disk.h"
#include "enc.h"
#include "stego.h"
#include <sys/types.h>

#define KEYTAB_TAG_SIZE 16
#define KEYTAB_ENTRY_SIZE                                                      \
  (ENC_KEY_SIZE + STEGO_LEVELS_PER_PASSWORD * STEGO_KEY_SIZE * 2 +             \
   KEYTAB_TAG_SIZE)
#define KEYTAB_SALT_SIZE 16

#define KEYTAB_SIZE                                                            \
  (KEYTAB_SALT_SIZE + KEYTAB_ENTRY_SIZE * STEGO_USER_LEVEL_COUNT)

int keytab_lookup(bs_disk_t disk, const char* password, stego_key_t* key);
int keytab_store(bs_disk_t disk, off_t index, const char* password,
                 const stego_key_t* key);

#endif // BS_KEYTAB_H
