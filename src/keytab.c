#include "keytab.h"

#include "enc.h"
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define KEYTAB_KEY_ENTRY_SIZE (KEYTAB_ENTRY_SIZE - KEYTAB_TAG_SIZE)
#define KEYTAB_KEY_IV_SIZE (ENC_AUTH_KEY_SIZE + ENC_AUTH_IV_SIZE)
#define KEYTAB_PBKDF_ITER 10000

static int gen_key_iv(const void* password, size_t password_len,
                      const void* salt, void* buf) {
  return enc_key_from_bytes(password, password_len, salt, KEYTAB_SALT_SIZE,
                            KEYTAB_PBKDF_ITER, KEYTAB_KEY_IV_SIZE, buf);
}

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
  uint8_t ent[KEYTAB_KEY_ENTRY_SIZE]; // Will hold serialized key

  // Search for key in table
  for (size_t i = 0; i < STEGO_USER_LEVEL_COUNT; i++) {
    const uint8_t* encrypted_ent = keytab_data + i * KEYTAB_ENTRY_SIZE;

    uint8_t key_iv[KEYTAB_KEY_IV_SIZE];
    int ret = gen_key_iv(password, password_len, salt, key_iv);
    if (ret < 0) {
      return ret;
    }

    ret = aes_decrypt_auth(key_iv, key_iv + ENC_AUTH_KEY_SIZE, encrypted_ent,
                           ent, KEYTAB_KEY_ENTRY_SIZE,
                           encrypted_ent + KEYTAB_KEY_ENTRY_SIZE,
                           KEYTAB_TAG_SIZE);

    if (ret == 0) {
      // Deserialize the key
      memcpy(key->aes_key, ent, sizeof(key->aes_key));
      memcpy(key->read_keys, ent + sizeof(key->aes_key),
             sizeof(key->read_keys));
      memcpy(key->write_keys,
             ent + sizeof(key->aes_key) + sizeof(key->read_keys),
             sizeof(key->write_keys));
    }

    if (ret != -EBADMSG) {
      // Either the key was found or there was an unknown error
      return ret;
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

  uint8_t ent[KEYTAB_KEY_ENTRY_SIZE]; // Will hold serialized key

  // Serialize the key
  memcpy(ent, key->aes_key, sizeof(key->aes_key));
  memcpy(ent + sizeof(key->aes_key), key->read_keys, sizeof(key->read_keys));
  memcpy(ent + sizeof(key->aes_key) + sizeof(key->read_keys), key->write_keys,
         sizeof(key->write_keys));

  uint8_t key_iv[KEYTAB_KEY_IV_SIZE];
  int ret = gen_key_iv(password, strlen(password), salt, key_iv);

  // Store the key in the key table
  uint8_t encrypted_ent[KEYTAB_ENTRY_SIZE];
  ret =
      aes_encrypt_auth(key_iv, key_iv + ENC_AUTH_KEY_SIZE, ent, encrypted_ent,
                       KEYTAB_KEY_ENTRY_SIZE,
                       encrypted_ent + KEYTAB_KEY_ENTRY_SIZE, KEYTAB_TAG_SIZE);
  if (ret < 0) {
    goto cleanup;
  }

  memcpy(keytab_data + index * KEYTAB_ENTRY_SIZE, encrypted_ent,
         KEYTAB_ENTRY_SIZE);

cleanup:
  disk_unlock_write(disk);
  return ret;
}
