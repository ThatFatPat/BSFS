#include "keytab.h"

static bool is_key_matching_index(const void* keytab_pointer,
                                  const void* password, int index) {

  const void* entry_pointer = get_pointer_from_index(keytab_pointer, index);

  void* buffer;
  size_t buffer_size;

  aes_decrypt(password, KEYTAB_KEY_SIZE, entry_pointer, KEYTAB_ENTRY_SIZE,
              &buffer, &buffer_size);

  for (int i = 0; i < KEYTAB_MAGIC_SIZE; i++) {
    if (((char*) buffer)[i] != KEYTAB_MAGIC[i]) {
      free(buffer);
      return false;
    }
  }

  free(buffer);
  return true;
}

static int get_key_index(const void* keytab_pointer, const void* password) {

  for (int i = 0; i < KEYTAB_LEN; i++) {
    if (is_key_matching_index(keytab_pointer, password, i)) {
      return i;
    }
  }

  return -1;
}

static const void* get_pointer_from_index(const void* keytab_pointer,
                                          int key_index) {
  return keytab_pointer + key_index * KEYTAB_ENTRY_SIZE; // check sizes
}

// value MUST be of the size KEYTAB_KEY_SIZE or only part of it will be actually
// written
static void change_keytab_val_with_key(void* keytab_pointer,
                                       const void* password,
                                       const void* value) {

  int key_index = get_key_index(keytab_pointer, password);
}

int keytab_lookup(bs_disk_t disk, const void* password, void* key) {

  const void* keytab_pointer;
  disk_lock_read(disk, &keytab_pointer);

  int key_index = get_key_index(keytab_pointer, password);

  if (key_index != -1) {
    char* enc_entry = (char*) get_pointer_from_index(keytab_pointer, key_index);

    void* buffer;
    size_t buffer_size;

    if (aes_decrypt(password, KEYTAB_KEY_SIZE, enc_entry, KEYTAB_ENTRY_SIZE,
                    &buffer, &buffer_size) != 0) {
      disk_unlock_read(disk);
      return -1;
    }

    key = buffer + KEYTAB_MAGIC_SIZE;

    free(buffer);
    disk_unlock_read(disk);

    return 0;
  }

  disk_unlock_read(disk);

  return -1;
}

int keytab_store(bs_disk_t disk, off_t index, const char* password,
                 const void* key) {

  void* keytab_pointer;
  disk_lock_write(disk, &keytab_pointer);

  char* data = KEYTAB_MAGIC;
  strcat(data, (const char*) key);
  // CHECK if concatenation works correctly

  void* data_pointer = &data;

  void* buffer;
  size_t buffer_size;

  if (aes_encrypt(password, (size_t) KEYTAB_KEY_SIZE, data_pointer,
                  (size_t) KEYTAB_ENTRY_SIZE, &buffer, &buffer_size) != 0) {
    disk_unlock_write(disk);
    return -1;
  };

  memcpy((void*) get_pointer_from_index(keytab_pointer, index), buffer,
         buffer_size);

  free(buffer);
  disk_unlock_write(disk);

  return 0;
}