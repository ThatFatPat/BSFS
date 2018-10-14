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

/**
 * Generating a random key.
 * The 2 last bytes are filled with 0.
 */
static int generate_random_key(uint8_t* buf) {
  if (RAND_bytes((unsigned char*) buf, STEGO_KEY_SIZE - 2) == 0) {
    return -EIO;
  }
  memset(buf + STEGO_KEY_SIZE - 2, 0, 2);
  return 0;
}

/**
 * Generating "count" orthonormal keys.
 * Uses special Gram-Schmidt to do so.
 */
int stego_gen_keys(void* buf, size_t count) {
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

    // If the norm of the key is 0, set the proper bit in the last two bytes.
    if (!vector_norm(key, STEGO_KEY_SIZE)) {
      set_bit(key + STEGO_KEY_SIZE - 2, i, 1);
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

off_t cover_offset(size_t disk_size, int i) {
  return KEYTAB_SIZE + i * compute_level_size(disk_size);
}

/**
 * Calculate the linear combination of all the cover files by the key in a
 * specific range (between off and off + size).
 * The result is also the multiplication of the key vector with
 * the cover files matrix.
 */
void ranged_covers_linear_combination(const void* key, const void* disk_data,
                                      size_t disk_size, off_t off, void* buf,
                                      size_t read_size) {
  const_vector_t data = (uint8_t*) disk_data;

  memset(buf, 0, read_size);
  for (int i = 0; i < COVER_FILE_COUNT; i++) {
    bool bit = get_bit(key, i);
    off_t offset = cover_offset(disk_size, i) + off;
    vector_linear_combination((vector_t) buf, (const_vector_t) buf,
                              data + offset, read_size, bit);
  }
}

static int write_level_encrypted(const void* key, bs_disk_t disk,
                                 const void* buf, off_t off, size_t size) {
  return -ENOSYS;
}

static bool check_parameters(size_t disk_size, off_t off, size_t buf_size) {
  return off < compute_level_size(disk_size) &&
         off + buf_size < compute_level_size(disk_size) && buf_size % 16 == 0;
}

int stego_read_level(const void* key, bs_disk_t disk, void* buf, off_t off,
                     size_t size) {
  if (!check_parameters(disk_get_size(disk), off, size)) {
    return -EINVAL;
  }

  int ret = 0;

  void* data = malloc(size);
  if (!data) {
    return -ENOMEM;
  }

  const void* disk_data;
  ret = disk_lock_read(disk, &disk_data);
  if (ret < 0) {
    goto cleanup_data;
  }

  ranged_covers_linear_combination(key, disk_data, disk_get_size(disk), off,
                                   data, size);

  disk_unlock_read(disk);

  void* decrypted;
  size_t decrypted_size;

  ret =
      aes_decrypt(key, STEGO_KEY_SIZE, data, size, &decrypted, &decrypted_size);
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
  return -ENOSYS;
}
