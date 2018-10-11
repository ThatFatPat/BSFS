#include "keytab.h"

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
      memcpy(key, (uint8_t*) ent + KEYTAB_MAGIC_SIZE, KEYTAB_KEY_SIZE);
      ret = 0;
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