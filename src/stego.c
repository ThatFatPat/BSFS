#include "stego.h"

#include "bit_util.h"
#include "keytab.h"
#include "vector.h"
#include <errno.h>
#include <openssl/rand.h>
#include <stdbool.h>
#include <string.h>

static size_t min(size_t a, size_t b) {
  return a < b ? a : b;
}

static size_t compute_level_size(size_t disk_size) {
  if (disk_size < KEYTAB_SIZE) {
    return 0;
  }
  return (disk_size - KEYTAB_SIZE) / STEGO_COVER_FILE_COUNT;
}

size_t stego_compute_user_level_size(size_t disk_size) {
  return STEGO_LEVELS_PER_PASSWORD * compute_level_size(disk_size);
}

static off_t cover_offset(size_t level_size, size_t i, off_t off) {
  return KEYTAB_SIZE + i * level_size + off;
}

/**
 * Calculate the linear combination of all the cover files by the key in a
 * specific range (between off and off + size).
 * The result is also the multiplication of the key vector with
 * the cover files matrix.
 */
static void read_cover_file_delta(const void* key, vector_t disk_data,
                                  size_t level_size, off_t off, vector_t buf,
                                  size_t read_size) {
  for (size_t i = 0; i < STEGO_COVER_FILE_COUNT; i++) {
    bool bit = get_bit(key, i);
    off_t cur_off = cover_offset(level_size, i, off);

    vector_linear_combination(buf, buf, disk_data + cur_off, read_size, bit);
  }
}

static void write_cover_file_delta(const void* key, vector_t disk_data,
                                   size_t level_size, off_t off, vector_t delta,
                                   size_t buf_size) {
  for (size_t i = 0; i < STEGO_COVER_FILE_COUNT; i++) {
    bool bit = get_bit(key, i);
    off_t cur_off = cover_offset(level_size, i, off);

    vector_linear_combination(disk_data + cur_off, disk_data + cur_off, delta,
                              buf_size, bit);
  }
}

static void read_merged_cover_file_delta(const stego_key_t* key,
                                         vector_t disk_data, size_t level_size,
                                         off_t off, vector_t buf, size_t size) {
  size_t level_idx = off / level_size;
  size_t bytes_read = 0;
  off_t cur_off = off % level_size;

  while (bytes_read < size) {
    size_t read_size = min(size - bytes_read, level_size - cur_off);
    read_cover_file_delta(key->read_keys[level_idx], disk_data, level_size,
                          cur_off, buf + bytes_read, read_size);
    cur_off = 0; // Reset cur_off after first iteration.
    bytes_read += read_size;
    level_idx++;
  }
}

static void write_merged_cover_file_delta(const stego_key_t* key,
                                          vector_t disk_data, size_t level_size,
                                          off_t off, vector_t delta,
                                          size_t size) {

  size_t level_idx = off / level_size;
  size_t bytes_written = 0;
  off_t cur_off = off % level_size;

  while (bytes_written < size) {
    size_t write_size = min(size - bytes_written, level_size - cur_off);
    write_cover_file_delta(key->write_keys[level_idx], disk_data, level_size,
                           cur_off, delta + bytes_written, write_size);
    cur_off = 0; // Reset cur_off after first iteration.
    bytes_written += write_size;
    level_idx++;
  }
}

static bool check_parameters(size_t user_level_size, off_t off,
                             size_t buf_size) {
  return (size_t) off < user_level_size && off + buf_size < user_level_size &&
         buf_size % 16 == 0 && off % 16 == 0;
}

int stego_read_level(const stego_key_t* key, bs_disk_t disk, void* buf,
                     off_t off, size_t size) {
  size_t disk_size = disk_get_size(disk);
  size_t user_level_size = stego_compute_user_level_size(disk_size);
  size_t level_size = compute_level_size(disk_size);

  if (!check_parameters(user_level_size, off, size)) {
    return -EINVAL;
  }

  void* data = malloc(size);
  if (!data) {
    return -ENOMEM;
  }

  const void* disk_data;
  int ret = disk_lock_read(disk, &disk_data);
  if (ret < 0) {
    goto cleanup_data;
  }

  memset(data, 0, size);

  // de-constification is safe as the data won't actually be modified
  read_merged_cover_file_delta(key, (vector_t) disk_data, level_size, off,
                               (vector_t) data, size);

  disk_unlock_read(disk);

  ret = aes_decrypt(key->aes_key, STEGO_AES_KEY_SIZE, data, buf, size);

cleanup_data:
  free(data);
  return ret;
}

int stego_write_level(const stego_key_t* key, bs_disk_t disk, const void* buf,
                      off_t off, size_t size) {
  size_t level_size = compute_level_size(disk_get_size(disk));

  if (!check_parameters(level_size, off, size)) {
    return -EINVAL;
  }

  void* encrypted = malloc(size);
  void* disk_data;

  int ret = aes_encrypt(key->aes_key, STEGO_AES_KEY_SIZE, buf, encrypted, size);
  if (ret < 0) {
    goto cleanup;
  }

  ret = disk_lock_write(disk, &disk_data);
  if (ret < 0) {
    goto cleanup;
  }

  // compute delta between existing disk contents and `encrypted`
  read_merged_cover_file_delta(key, (vector_t) disk_data, level_size, off,
                               (vector_t) encrypted, size);

  // write to disk
  write_merged_cover_file_delta(key, (vector_t) disk_data, level_size, off,
                                (vector_t) encrypted, size);

  disk_unlock_write(disk);

cleanup:
  free(encrypted);
  return ret;
}
