#include "stego.h"

#include "bit_util.h"
#include "keytab.h"
#include "vector.h"
#include <errno.h>
#include <openssl/rand.h>
#include <stdbool.h>
#include <string.h>

#define STEGO_USER_KEY_SIZE (STEGO_KEY_SIZE * STEGO_LEVELS_PER_PASSWORD)

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

int stego_gen_user_keys(stego_key_t* keys, size_t count) {
  if (count > STEGO_USER_LEVEL_COUNT) {
    return -EINVAL;
  }

  uint8_t read_keys[round_to_bytes(STEGO_COVER_FILE_COUNT *
                                   STEGO_COVER_FILE_COUNT)];
  uint8_t write_keys[sizeof(read_keys)];

  int ret = matrix_gen_nonsing(read_keys, write_keys, STEGO_COVER_FILE_COUNT);
  if (ret < 0) {
    return ret;
  }

  matrix_transpose(write_keys, write_keys, STEGO_COVER_FILE_COUNT);

  for (size_t i = 0; i < count; i++) {
    if (!RAND_bytes(keys[i].aes_key, STEGO_AES_KEY_SIZE)) {
      return -EIO;
    }
    memcpy(keys[i].read_keys, read_keys + i * STEGO_USER_KEY_SIZE,
           STEGO_USER_KEY_SIZE);
    memcpy(keys[i].write_keys, write_keys + i * STEGO_USER_KEY_SIZE,
           STEGO_USER_KEY_SIZE);
  }

  return 0;
}

/// READ/WRITE IMPLEMENTATION

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
                                  size_t buf_size) {
  for (size_t i = 0; i < STEGO_COVER_FILE_COUNT; i++) {
    bool bit = get_bit(key, i);
    off_t cur_off = cover_offset(level_size, i, off);

    vector_linear_combination(buf, buf, disk_data + cur_off, buf_size, bit);
  }
}

static void write_cover_file_delta(const void* key, vector_t disk_data,
                                   size_t level_size, off_t off, vector_t buf,
                                   size_t buf_size) {
  for (size_t i = 0; i < STEGO_COVER_FILE_COUNT; i++) {
    bool bit = get_bit(key, i);
    off_t cur_off = cover_offset(level_size, i, off);

    vector_linear_combination(disk_data + cur_off, disk_data + cur_off, buf,
                              buf_size, bit);
  }
}

typedef uint8_t individual_key_t[STEGO_KEY_SIZE];
typedef void (*cover_file_op_t)(const void*, vector_t, size_t, off_t, vector_t,
                                size_t);

static void merged_cover_file_op(cover_file_op_t op,
                                 const individual_key_t* keys, vector_t disk,
                                 size_t level_size, off_t off, vector_t buf,
                                 size_t size) {
  size_t level_idx = off / level_size;
  off_t cur_off = off % level_size;

  size_t processed = 0;
  while (processed < size) {
    size_t total_remaining = size - processed;
    size_t level_remaining = level_size - cur_off;
    size_t cur_size = min(total_remaining, level_remaining);

    op(keys[level_idx], disk, level_size, cur_off, buf + processed, cur_size);

    cur_off = 0; // We always operate from offset 0 after the first iteration.
    processed += cur_size;
    level_idx++;
  }
}

static void read_merged_cover_file_delta(const stego_key_t* key,
                                         const void* disk, size_t level_size,
                                         off_t off, void* buf, size_t size) {
  // De-constification of `disk` is safe as `read_cover_file_delta` will not
  // modify it.
  merged_cover_file_op(read_cover_file_delta, key->read_keys, (vector_t) disk,
                       level_size, off, (vector_t) buf, size);
}

static void write_merged_cover_file_delta(const stego_key_t* key, void* disk,
                                          size_t level_size, off_t off,
                                          const void* buf, size_t size) {
  // De-constification of `buf` is safe as `write_cover_file_delta` will not
  // modify it.
  merged_cover_file_op(write_cover_file_delta, key->write_keys, (vector_t) disk,
                       level_size, off, (vector_t) buf, size);
}

static bool check_parameters(size_t user_level_size, off_t off,
                             size_t buf_size) {
  return (size_t) off < user_level_size && buf_size < user_level_size - off &&
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

  read_merged_cover_file_delta(key, disk_data, level_size, off, data, size);

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
  read_merged_cover_file_delta(key, disk_data, level_size, off, encrypted,
                               size);

  // write to disk
  write_merged_cover_file_delta(key, disk_data, level_size, off, encrypted,
                                size);

  disk_unlock_write(disk);

cleanup:
  free(encrypted);
  return ret;
}
