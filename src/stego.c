#include "stego.h"

#include "bit_util.h"
#include "keytab.h"
#include "vector.h"
#include <errno.h>
#include <openssl/rand.h>
#include <stdbool.h>
#include <string.h>

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
static void read_cover_file_delta(const void* key, const void* disk_data,
                                  size_t level_size, off_t off, void* buf,
                                  size_t read_size) {
  const_vector_t data = (const_vector_t) disk_data;

  for (size_t i = 0; i < STEGO_COVER_FILE_COUNT; i++) {
    bool bit = get_bit(key, i);
    off_t cur_off = cover_offset(level_size, i, off);

    vector_linear_combination((vector_t) buf, (const_vector_t) buf,
                              data + cur_off, read_size, bit);
  }
}

static void write_cover_file_delta(const void* key, void* disk_data,
                                   size_t level_size, off_t off, void* delta,
                                   size_t buf_size) {
  vector_t data = (vector_t) disk_data;

  for (size_t i = 0; i < STEGO_COVER_FILE_COUNT; i++) {
    bool bit = get_bit(key, i);
    off_t cur_off = cover_offset(level_size, i, off);

    vector_linear_combination(data + cur_off, data + cur_off,
                              (const_vector_t) delta, buf_size, bit);
  }
}

static bool check_parameters(size_t level_size, off_t off, size_t buf_size) {
  return (size_t) off < level_size && off + buf_size < level_size &&
         buf_size % 16 == 0 && off % 16 == 0;
}

int stego_read_level(const void* key, bs_disk_t disk, void* buf, off_t off,
                     size_t size) {
  size_t level_size = compute_level_size(disk_get_size(disk));

  if (!check_parameters(level_size, off, size)) {
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
  read_cover_file_delta(key, disk_data, level_size, off, data, size);

  disk_unlock_read(disk);

  ret = aes_decrypt(key, STEGO_KEY_SIZE, data, buf, size);

cleanup_data:
  free(data);
  return ret;
}

int stego_write_level(const void* key, bs_disk_t disk, const void* buf,
                      off_t off, size_t size) {
  size_t level_size = compute_level_size(disk_get_size(disk));

  if (!check_parameters(level_size, off, size)) {
    return -EINVAL;
  }

  void* encrypted = malloc(size);
  void* disk_data;

  int ret = aes_encrypt(key, STEGO_KEY_SIZE, buf, encrypted, size);
  if (ret < 0) {
    goto cleanup;
  }

  ret = disk_lock_write(disk, &disk_data);
  if (ret < 0) {
    goto cleanup;
  }

  // compute delta between existing disk contents and `encrypted`
  read_cover_file_delta(key, disk_data, level_size, off, encrypted, size);

  // write to disk
  write_cover_file_delta(key, disk_data, level_size, off, encrypted, size);

  disk_unlock_write(disk);

cleanup:
  free(encrypted);
  return ret;
}
