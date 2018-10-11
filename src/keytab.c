#include "keytab.h"

#include "enc.h"
#include <arpa/inet.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define KEYTAB_MAGIC 0xBEEFCAFE
#define KEYTAB_MAGIC_SIZE (sizeof KEYTAB_MAGIC)
#define KEYTAB_KEY_SIZE 1

static const void* get_pointer_from_index(const void* keytab_pointer,
                                          int key_index) {
  return keytab_pointer + key_index * KEYTAB_ENTRY_SIZE; // check sizes
}

static bool is_key_matching_index(const void* keytab_pointer,
                                  const void* password, int index) {

  const void* entry_pointer = get_pointer_from_index(keytab_pointer, index);

  void* buffer;
  size_t buffer_size;

  aes_decrypt(password, KEYTAB_KEY_SIZE, entry_pointer, KEYTAB_ENTRY_SIZE,
              &buffer, &buffer_size);

  for (int i = 0; i < KEYTAB_MAGIC_SIZE; i++) {
    if (((char*) buffer)[i] != ((unsigned char*) KEYTAB_MAGIC)[i]) {
      free(buffer);
      return false;
    }
  }

  free(buffer);
  return true;
}

static int get_key_index(const void* keytab_pointer, const void* password) {

  for (int i = 0; i < KEYTAB_MAX_LEVELS; i++) {
    if (is_key_matching_index(keytab_pointer, password, i)) {
      return i;
    }
  }

  return -1;
}

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

    memcpy(key, buffer + KEYTAB_MAGIC_SIZE, KEYTAB_KEY_SIZE);

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

  unsigned char* data =
      (unsigned char*) malloc(KEYTAB_MAGIC_SIZE + KEYTAB_KEY_SIZE);
  memcpy(data, (const unsigned char*) KEYTAB_MAGIC, KEYTAB_MAGIC_SIZE);
  data += KEYTAB_MAGIC_SIZE;
  memcpy(data, (const char*) key, KEYTAB_KEY_SIZE);
  data -= KEYTAB_MAGIC_SIZE;
  // CHECK if concatenation works correctly

  void* buffer;
  size_t buffer_size;

  if (aes_encrypt(password, (size_t) KEYTAB_KEY_SIZE, data,
                  (size_t) KEYTAB_ENTRY_SIZE, &buffer, &buffer_size) != 0) {
    free(data);
    disk_unlock_write(disk);
    return -1;
  };

  memcpy((void*) get_pointer_from_index(keytab_pointer, index), buffer,
         buffer_size);

  free(data);
  free(buffer);
  disk_unlock_write(disk);

  return 0;
}