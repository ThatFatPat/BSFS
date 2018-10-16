#include "keytab.h"

#include "bit_util.h"
#include "enc.h"
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define KEYTAB_MAGIC 0xBEEFCAFE
#define KEYTAB_KEY_SIZE 16 // TODO: use STEGO_KEY_SIZE
#define KEYTAB_TAG_SIZE 16

int keytab_lookup(bs_disk_t disk, const char* password, void* key) {
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

  for (size_t i = 0; i < MAX_LEVELS; i++) {
    const uint8_t* encrypted_ent = keytab_data + i * KEYTAB_ENTRY_SIZE;

    int decrypt_status = aes_decrypt_auth(
        password, password_len, salt, KEYTAB_SALT_SIZE, encrypted_ent, key,
        KEYTAB_KEY_SIZE, encrypted_ent + KEYTAB_KEY_SIZE, KEYTAB_TAG_SIZE);

    if (decrypt_status != -EBADMSG) {
      return decrypt_status;
    }
  }

  return -ENOENT;
}

int keytab_store(bs_disk_t disk, off_t index, const char* password,
                 const void* key) {
  if (index >= MAX_LEVELS) {
    return -EINVAL;
  }

  // Unlocking and re-locking the disk later is safe, as the salt shouldn't
  // change anyway (other things would break if it did).
  uint8_t salt[KEYTAB_SALT_SIZE];
  {
    const void* readable_keytab;
    int lock_status = disk_lock_read(disk, &readable_keytab);
    if (lock_status < 0) {
      return lock_status;
    }
    memcpy(salt, readable_keytab, KEYTAB_SALT_SIZE);
    disk_unlock_read(disk);
  }

  uint8_t ent[KEYTAB_ENTRY_SIZE];
  int ret = aes_encrypt_auth(password, strlen(password), salt, KEYTAB_SALT_SIZE,
                             key, ent, KEYTAB_KEY_SIZE, ent + KEYTAB_KEY_SIZE,
                             KEYTAB_TAG_SIZE);
  if (ret < 0) {
    return ret;
  }

  void* keytab;
  ret = disk_lock_write(disk, &keytab);
  if (ret < 0) {
    return ret;
  }

  memcpy((uint8_t*) keytab + KEYTAB_SALT_SIZE + index * KEYTAB_ENTRY_SIZE, ent,
         KEYTAB_ENTRY_SIZE);

  disk_unlock_write(disk);
  return 0;
}
