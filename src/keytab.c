#include "keytab.h"

void* get_keytab_pointer() {
  return 0;
}

bool is_key_matching_index(const void* password, int index) {

  void* entry_pointer = get_pointer_from_index(index);

  void* buffer;
  size_t buffer_size;

  aes_decrypt(password, KEYTAB_KEY_SIZE, entry_pointer, KEYTAB_ENTRY_SIZE,
              &buffer, &buffer_size);

  for (int i = 0; i < KEYTAB_MAGIC_SIZE; i++) {
    if (((char*) buffer)[i] != KEYTAB_MAGIC[i]) {
      return false;
    }
  }

  return true;
}

int get_key_index(const void* password) {

  for (int i = 0; i < KEYTAB_LEN; i++) {
    if (is_key_matching_index(password, i)) {
      return i;
    }
  }

  return -1;
}

void* get_pointer_from_index(int key_index) {
  return get_keytab_pointer() + key_index * KEYTAB_ENTRY_SIZE;
}

// value MUST be of the size KEYTAB_KEY_SIZE or only part of it will be actually
// written
void change_keytab_val_with_key(const void* password, const void* value) {

  int key_index = get_key_index(password);

  if (key_index != -1) {
    // password is correct

    char* data =
        KEYTAB_MAGIC; // TODO (concatinate the magic number with the value)
    void* data_pointer = &data;

    void* buffer;
    size_t buffer_size;

    aes_encrypt(password, (size_t) KEYTAB_KEY_SIZE, data_pointer,
                (size_t) KEYTAB_ENTRY_SIZE, &buffer, &buffer_size);

    *(char*) get_pointer_from_index(key_index) =
        *(char*) buffer; // CHECK IF WORKS CORRECTLY
  }
}

// before calling this, make sure that get_key_index(password) != -1
// otherwise will always return 0
char* get_next_key(const void* password) {

  void* keytab_pointer = get_keytab_pointer();

  int key_index = get_key_index(password);

  if (key_index != -1) {
    return (char*) ((int) keytab_pointer +
                    (int) get_pointer_from_index(
                        key_index)); // CHECK IF INT SIZE IS CORRECT
  }

  return 0;
}