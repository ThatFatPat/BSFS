#include "keytab.h"

#include "enc.h"
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define KEYTAB_KEY_ENTRY_SIZE (KEYTAB_ENTRY_SIZE - KEYTAB_TAG_SIZE)

int keytab_lookup(bs_disk_t disk, const char* password, stego_key_t* key) {
  // Get the key table
  uint8_t keytab[KEYTAB_SIZE];

  {
    const void* disk_keytab;
    int lock_status = disk_lock_read(disk, &disk_keytab);
    if (lock_status < 0) {
      return lock_status;
    }
    memcpy(keytab, disk_keytab, KEYTAB_SIZE);
    disk_unlock_read(disk);
  }

  const uint8_t* salt = keytab;
  const uint8_t* keytab_data = keytab + KEYTAB_SALT_SIZE;

  size_t password_len = strlen(password);
  uint8_t key_buf[KEYTAB_KEY_ENTRY_SIZE]; // Will hold serialized key

  // Search for key in table
  for (size_t i = 0; i < STEGO_USER_LEVEL_COUNT; i++) {
    const uint8_t* encrypted_ent = keytab_data + i * KEYTAB_ENTRY_SIZE;

    int decrypt_status = aes_decrypt_auth(
        password, password_len, salt, KEYTAB_SALT_SIZE, encrypted_ent, key_buf,
        KEYTAB_KEY_ENTRY_SIZE, encrypted_ent + KEYTAB_KEY_ENTRY_SIZE,
        KEYTAB_TAG_SIZE);

    if (decrypt_status == 0) {
      // Deserialize the key
      memcpy(key->aes_key, key_buf, sizeof(key->aes_key));
      memcpy(key->read_keys, key_buf + sizeof(key->aes_key),
             sizeof(key->read_keys));
      memcpy(key->write_keys,
             key_buf + sizeof(key->aes_key) + sizeof(key->read_keys),
             sizeof(key->write_keys));
    }

    if (decrypt_status != -EBADMSG) {
      // Either the key was found or there was an unknown error
      return decrypt_status;
    }
  }

  return -ENOENT;
}

int keytab_store(bs_disk_t disk, off_t index, const char* password,
                 const stego_key_t* key) {
  if (index >= STEGO_USER_LEVEL_COUNT) {
    return -EINVAL;
  }

  // Get the key table
  void* keytab;
  int lock_status = disk_lock_write(disk, &keytab);
  if (lock_status < 0) {
    return lock_status;
  }

  const void* salt = keytab;
  uint8_t* keytab_data = (uint8_t*) keytab + KEYTAB_SALT_SIZE;

  uint8_t key_buf[KEYTAB_KEY_ENTRY_SIZE]; // Will hold serialized key

  // Serialize the key
  memcpy(key_buf, key->aes_key, sizeof(key->aes_key));
  memcpy(key_buf + sizeof(key->aes_key), key->read_keys,
         sizeof(key->read_keys));
  memcpy(key_buf + sizeof(key->aes_key) + sizeof(key->read_keys),
         key->write_keys, sizeof(key->write_keys));

  // Store the key in the key table
  uint8_t ent[KEYTAB_ENTRY_SIZE];
  int ret = aes_encrypt_auth(password, strlen(password), salt, KEYTAB_SALT_SIZE,
                             key_buf, ent, KEYTAB_KEY_ENTRY_SIZE,
                             ent + KEYTAB_KEY_ENTRY_SIZE, KEYTAB_TAG_SIZE);
  if (ret < 0) {
    goto cleanup;
  }

  memcpy(keytab_data + index * KEYTAB_ENTRY_SIZE, ent, KEYTAB_ENTRY_SIZE);

cleanup:
  disk_unlock_write(disk);
  return ret;
}
