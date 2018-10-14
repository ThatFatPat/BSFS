#include "keytab.h"

#include "bit_util.h"
#include "enc.h"
#include <arpa/inet.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define KEYTAB_MAGIC 0xBEEFCAFE
#define KEYTAB_KEY_SIZE 16 // TODO: use STEGO_KEY_SIZE
#define KEYTAB_MAGIC_SIZE sizeof(uint32_t)
#define KEYTAB_ACTUAL_ENTRY_SIZE (KEYTAB_KEY_SIZE + KEYTAB_MAGIC_SIZE)

int keytab_lookup(bs_disk_t disk, const char* password, void* key) {
  uint8_t keytab[KEYTAB_ENTRY_SIZE * KEYTAB_MAX_LEVELS];

  {
    const void* disk_keytab;
    int lock_status = disk_lock_read(disk, &disk_keytab);
    if (lock_status < 0) {
      return lock_status;
    }
    memcpy(keytab, disk_keytab, KEYTAB_ENTRY_SIZE * KEYTAB_MAX_LEVELS);
    disk_unlock_read(disk);
  }

  int ret = -ENOENT;
  size_t password_len = strlen(password);

  for (size_t i = 0; i < KEYTAB_MAX_LEVELS; i++) {
    const void* encrypted_ent = keytab + i * KEYTAB_ENTRY_SIZE;

    void* ent = NULL;
    size_t ent_size;
    int decrypt_status = aes_decrypt(password, password_len, encrypted_ent,
                                     KEYTAB_ENTRY_SIZE, &ent, &ent_size);

    if (decrypt_status == 0 && ent_size == KEYTAB_ACTUAL_ENTRY_SIZE &&
        read_big_endian(ent) == KEYTAB_MAGIC) {
      memcpy(key, (uint8_t*) ent + KEYTAB_MAGIC_SIZE, KEYTAB_KEY_SIZE);
      ret = 0;
      free(ent);
      break;
    }

    if (decrypt_status < 0 && decrypt_status != -EIO) {
      ret = decrypt_status;
      free(ent);
      break;
    }

    free(ent);
  }

  return ret;
}

int keytab_store(bs_disk_t disk, off_t index, const char* password,
                 const void* key) {
  if (index >= KEYTAB_MAX_LEVELS) {
    return -EINVAL;
  }

  int ret = 0;

  uint8_t ent[KEYTAB_ACTUAL_ENTRY_SIZE];
  write_big_endian(ent, KEYTAB_MAGIC);
  memcpy(ent + KEYTAB_MAGIC_SIZE, key, KEYTAB_KEY_SIZE);

  void* encrypted_ent;
  size_t encrypted_size;

  ret = aes_encrypt(password, strlen(password), ent, KEYTAB_ACTUAL_ENTRY_SIZE,
                    &encrypted_ent, &encrypted_size);
  if (ret < 0) {
    return ret;
  }
  if (encrypted_size != KEYTAB_ENTRY_SIZE) {
    // technically, this should be impossible
    ret = -EIO;
    goto cleanup;
  }

  void* keytab;
  ret = disk_lock_write(disk, &keytab);
  if (ret < 0) {
    goto cleanup;
  }

  memcpy((uint8_t*) keytab + index * KEYTAB_ENTRY_SIZE, encrypted_ent,
         KEYTAB_ENTRY_SIZE);

  disk_unlock_write(disk);

cleanup:
  free(encrypted_ent);
  return ret;
}
