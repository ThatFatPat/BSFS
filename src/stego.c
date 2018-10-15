#include "stego.h"

#include "bit_util.h"
#include "keytab.h"
#include "vector.h"
#include <errno.h>
#include <openssl/rand.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define KEYTAB_SIZE (KEYTAB_ENTRY_SIZE * MAX_LEVELS)

// size of padding appended to every key, used to ensure orthonormality
#define STEGO_KEY_PADDING_SIZE (MAX_LEVELS / CHAR_BIT)

// size of the random part of the key
#define STEGO_KEY_RND_SIZE (STEGO_KEY_SIZE - STEGO_KEY_PADDING_SIZE)

/**
 * Generating a random key.
 * The 2 last bytes are filled with 0.
 */
static int generate_random_key(uint8_t* buf) {
  if (!RAND_bytes(buf, STEGO_KEY_RND_SIZE)) {
    return -EIO;
  }
  memset(buf + STEGO_KEY_RND_SIZE, 0, STEGO_KEY_PADDING_SIZE);
  return 0;
}

/**
 * Generating "count" orthonormal keys.
 * Uses special Gram-Schmidt to do so.
 */
int stego_gen_keys(void* buf, size_t count) {
  if (count > MAX_LEVELS) {
    return -EINVAL;
  }

  uint8_t* int_buf = (uint8_t*) buf;

  for (size_t i = 0; i < count; i++) {
    vector_t key = int_buf + i * STEGO_KEY_SIZE;

    int rnd_status = generate_random_key(key);
    if (rnd_status < 0) {
      return rnd_status;
    }

    // Add to "key" all the keys before him which have scalar product 1 with
    // him.
    for (size_t j = 0; j < i; j++) {
      vector_t other_key = int_buf + j * STEGO_KEY_SIZE;

      vector_linear_combination(
          key, key, other_key, STEGO_KEY_SIZE,
          vector_scalar_product(key, other_key, STEGO_KEY_SIZE));
    }

    // If the norm of the key is 0, set the proper bit in the last padding
    // bytes.
    if (!vector_norm(key, STEGO_KEY_SIZE)) {
      set_bit(key + STEGO_KEY_RND_SIZE, i, 1);
    }
  }

  return 0;
}

size_t compute_level_size(size_t disk_size) {
  if (disk_size < KEYTAB_SIZE) {
    return 0;
  }
  return (disk_size - KEYTAB_SIZE) / COVER_FILE_COUNT;
}

static off_t cover_offset(size_t disk_size, int i) {
  return KEYTAB_SIZE + i * compute_level_size(disk_size);
}

/**
 * Calculate the linear combination of all the cover files by the key in a
 * specific range (between off and off + size).
 * The result is also the multiplication of the key vector with
 * the cover files matrix.
 */
static void ranged_covers_linear_combination(const void* key,
                                             const void* disk_data,
                                             size_t disk_size, off_t off,
                                             void* buf, size_t read_size) {
  const_vector_t data = (uint8_t*) disk_data;

  for (int i = 0; i < COVER_FILE_COUNT; i++) {
    bool bit = get_bit(key, i);
    off_t offset = cover_offset(disk_size, i) + off;
    vector_linear_combination((vector_t) buf, (const_vector_t) buf,
                              data + offset, read_size, bit);
  }
}

static void write_cover_files(const void* key, void* disk_data,
                              size_t disk_size, off_t off, void* delta,
                              size_t buf_size) {
  vector_t data = (vector_t) disk_data;

  for (size_t i = 0; i < COVER_FILE_COUNT; i++) {
    bool bit = get_bit(key, i);
    off_t offset = cover_offset(disk_size, i) + off;
    vector_linear_combination(data + offset, data + offset,
                              (const_vector_t) delta, buf_size, bit);
  }
}

static bool check_parameters(size_t disk_size, off_t off, size_t buf_size) {
  return (size_t) off < compute_level_size(disk_size) &&
         off + buf_size < compute_level_size(disk_size) && off % 16 == 0;
}

int stego_read_level(const void* key, bs_disk_t disk, void* buf, off_t off,
                     size_t size) {
  if (!check_parameters(disk_get_size(disk), off, size)) {
    return -EINVAL;
  }

  size_t encrypted_size = aes_get_encrypted_size(size);

  void* data = malloc(encrypted_size);
  if (!data) {
    return -ENOMEM;
  }

  const void* disk_data;
  int ret = disk_lock_read(disk, &disk_data);
  if (ret < 0) {
    goto cleanup_data;
  }

  memset(data, 0, encrypted_size);
  ranged_covers_linear_combination(key, disk_data, disk_get_size(disk), off,
                                   data, encrypted_size);

  disk_unlock_read(disk);

  void* decrypted;
  size_t decrypted_size;

  ret = aes_decrypt(key, STEGO_KEY_SIZE, data, encrypted_size, &decrypted,
                    &decrypted_size);
  if (ret < 0) {
    goto cleanup_data;
  }
  if (decrypted_size != size) {
    ret = -EIO;
    goto cleanup_decrypted;
  }

  memcpy(buf, decrypted, size);

cleanup_decrypted:
  free(decrypted);
cleanup_data:
  free(data);
  return ret;
}

int stego_write_level(const void* key, bs_disk_t disk, const void* buf,
                      off_t off, size_t size) {
  if (!check_parameters(disk_get_size(disk), off, size)) {
    return -EINVAL;
  }

  void* encrypted;
  size_t encrypted_size;

  void* disk_data;

  int ret =
      aes_encrypt(key, STEGO_KEY_SIZE, buf, size, &encrypted, &encrypted_size);
  if (ret < 0) {
    return ret;
  }

  ret = disk_lock_write(disk, &disk_data);
  if (ret < 0) {
    goto cleanup;
  }

  // compute delta between existing disk contents and `encrypted`
  ranged_covers_linear_combination(key, disk_data, disk_get_size(disk), off,
                                   encrypted, encrypted_size);

  // write to disk
  write_cover_files(key, disk_data, disk_get_size(disk), off, encrypted,
                    encrypted_size);

  disk_unlock_write(disk);

cleanup:
  free(encrypted);
  return ret;
}
