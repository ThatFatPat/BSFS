#ifndef BS_KEYTAB_H
#define BS_KEYTAB_H
#include "disk.h"
#include "enc.h"
#include <errno.h>
#include <openssl/aes.h>
#include <openssl/evp.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>

#define KEYTAB_MAGIC "BEEFCAFE"
#define KEYTAB_MAGIC_SIZE (sizeof KEYTAB_MAGIC)
#define KEYTAB_LEN 1
#define KEYTAB_KEY_SIZE 1
#define KEYTAB_ENCRIPTION_KEY_SIZE 1
#define KEYTAB_ENTRY_SIZE (KEYTAB_ENCRIPTION_KEY_SIZE + KEYTAB_MAGIC_SIZE)
#define KEYTAB_TOTALSIZE (KEYTAB_LEN * KEYTAB_VAL_SIZE)

static bool is_key_matching_index(const void* keytab_pointer,
                                  const void* password, int index);
static int get_key_index(const void* keytab_pointer, const void* password);
static const void* get_pointer_from_index(const void* keytab_pointer,
                                          int key_index);
// might not be required
static void change_keytab_val_with_key(void* keytab_pointer,
                                       const void* password, const void* value);
// might not be required

// known api
int keytab_lookup(bs_disk_t disk, const void* password, void* key);
int keytab_store(bs_disk_t disk, off_t index, const char* password,
                 const void* key);

#endif // BS_KEYTAB_H
