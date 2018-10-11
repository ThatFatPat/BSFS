#include "keytab.h"

#include "enc.h"
#include <arpa/inet.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define KEYTAB_MAGIC 0xBEEFCAFE
#define KEYTAB_KEY_SIZE 16
#define KEYTAB_ACTUAL_ENTRY_SIZE KEYTAB_KEY_SIZE + sizeof(uint32_t)

static uint32_t read_big_endian(const void* buf) {
  uint32_t big_endian;
  memcpy(&big_endian, buf, sizeof(uint32_t));
  return ntohl(big_endian);
}

static void write_big_endian(void* buf, uint32_t host_endian) {
  uint32_t big_endian = htonl(host_endian);
  memcpy(buf, &big_endian, sizeof(uint32_t));
}

int keytab_lookup(bs_disk_t disk, const char* password, void* key) {
  int ret = -ENOENT;
  size_t password_len = strlen(password);

  const void* keytab;
  int lock_status = disk_lock_read(disk, &keytab);
  if (lock_status < 0) {
    return lock_status;
  }

  for (size_t i = 0; i < KEYTAB_MAX_LEVELS; i++) {
    const uint8_t* encrypted_ent =
        (const uint8_t*) keytab + i * KEYTAB_ENTRY_SIZE;

    void* ent;
    size_t ent_size;
    int decrypt_status = aes_decrypt(password, password_len, encrypted_ent,
                                     KEYTAB_ENTRY_SIZE, &ent, &ent_size);
    if (decrypt_status < 0) {
      ret = decrypt_status;
      break;
    }
    if (ent_size != KEYTAB_ACTUAL_ENTRY_SIZE) {
      ret = -EINVAL;
      free(ent);
      break;
    }

    if (read_big_endian(ent) == KEYTAB_MAGIC) {
      memcpy(key, (uint8_t*) ent + sizeof(uint32_t), KEYTAB_KEY_SIZE);
      ret = 0;
      free(ent);
      break;
    }

    free(ent);
  }

  disk_unlock_read(disk);
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
  memcpy(ent + sizeof(uint32_t), key, KEYTAB_KEY_SIZE);

  void* encrypted_ent;
  size_t encrypted_size;

  ret = aes_encrypt(password, strlen(password), ent, KEYTAB_ACTUAL_ENTRY_SIZE,
                    &encrypted_ent, &encrypted_size);
  if (ret < 0) {
    return ret;
  }

  void* keytab;
  ret = disk_lock_write(disk, &keytab);
  if (ret < 0) {
    goto cleanup;
  }

  memcpy((uint8_t*) keytab + index * KEYTAB_ENTRY_SIZE, encrypted_ent,
         KEYTAB_ENTRY_SIZE);

cleanup:
  free(encrypted_ent);
  disk_unlock_write(disk);
  return ret;
}