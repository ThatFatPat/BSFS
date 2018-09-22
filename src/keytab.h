#ifndef BS_KEYTAB_H
#define BS_KEYTAB_H

#define KEYTAB_MAGIC "DEADBEEF"
#define KEYTAB_MAGIC_SIZE (sizeof KEYTAB_MAGIC)
#define KEYTAB_LEN 1
#define KEYTAB_KEY_SIZE 1
#define KEYTAB_ENCRIPTION_KEY_SIZE 1
#define KEYTAB_ENTRY_SIZE (KEYTAB_ENCRIPTION_KEY_SIZE + KEYTAB_MAGIC_SIZE)
#define KEYTAB_TOTALSIZE (KEYTAB_LEN * KEYTAB_VAL_SIZE)

void* get_keytab_pointer();
bool is_key_matching_index(const void* password, int index);
int get_key_index(const void* password);
void* get_pointer_from_index(int key_index);
void change_keytab_val_with_key(const void* password, const void* value);
char* get_next_key(const void* password);

#include "enc.c"
#include <stdbool.h>
#include <stddef.h>

#endif // BS_KEYTAB_H
