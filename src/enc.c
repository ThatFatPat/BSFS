#include "enc.h"

#include <assert.h>
#include <errno.h>
#include <openssl/aes.h>
#include <openssl/evp.h>
#include <stdint.h>
#include <string.h>

#define NROUNDS 10000

static_assert(ENC_BLOCK_SIZE == AES_BLOCK_SIZE,
              "Incorrect block size constant");

static int
init_cipher_ctx(const EVP_CIPHER* cipher, const void* key, const void* iv,
                int (*init_func)(EVP_CIPHER_CTX*, const EVP_CIPHER*, ENGINE*,
                                 const uint8_t*, const uint8_t*),
                EVP_CIPHER_CTX** ctx) {
  EVP_CIPHER_CTX* tmp_ctx = EVP_CIPHER_CTX_new();
  if (!tmp_ctx) {
    return -ENOMEM;
  }

  if (!init_func(tmp_ctx, cipher, NULL, key, iv)) {
    EVP_CIPHER_CTX_free(tmp_ctx);
    return -EIO;
  }
  EVP_CIPHER_CTX_set_padding(tmp_ctx, 0);

  *ctx = tmp_ctx;
  return 0;
}

int enc_key_from_bytes(const void* password, size_t password_size,
                       const void* salt, size_t salt_size, int iter,
                       size_t key_size, void* key) {
  return PKCS5_PBKDF2_HMAC((const char*) password, password_size,
                           (const uint8_t*) salt, salt_size, iter, EVP_sha256(),
                           key_size, key)
             ? 0
             : -EIO;
}

int aes_encrypt(const void* key, const void* iv, const void* plain, void* enc,
                size_t size) {
  if (size % ENC_BLOCK_SIZE != 0) {
    return -EINVAL;
  }

  EVP_CIPHER_CTX* e_ctx;
  int ret =
      init_cipher_ctx(EVP_aes_128_cbc(), key, iv, EVP_EncryptInit_ex, &e_ctx);
  if (ret < 0) {
    return ret;
  }

  int content_size = 0;
  int final_size = 0;

  if (!EVP_EncryptUpdate(e_ctx, (uint8_t*) enc, &content_size,
                         (const uint8_t*) plain, size)) {
    ret = -EIO;
    goto cleanup;
  }

  if (!EVP_EncryptFinal_ex(e_ctx, (uint8_t*) enc + content_size, &final_size)) {
    ret = -EIO;
    goto cleanup;
  }

  if ((size_t)(content_size + final_size) != size) {
    // technically, this should be impossible
    ret = -EIO;
  }

cleanup:
  EVP_CIPHER_CTX_free(e_ctx);
  return ret;
}

int aes_decrypt(const void* key, const void* iv, const void* enc, void* plain,
                size_t size) {
  if (size % ENC_BLOCK_SIZE != 0) {
    return -EINVAL;
  }

  EVP_CIPHER_CTX* d_ctx;
  int ret =
      init_cipher_ctx(EVP_aes_128_cbc(), key, iv, EVP_DecryptInit_ex, &d_ctx);
  if (ret < 0) {
    return ret;
  }

  int content_size = 0;
  int final_size = 0;

  if (!EVP_DecryptUpdate(d_ctx, (uint8_t*) plain, &content_size,
                         (const uint8_t*) enc, size)) {
    ret = -EIO;
    goto cleanup;
  }

  if (!EVP_DecryptFinal_ex(d_ctx, (uint8_t*) plain + content_size,
                           &final_size)) {
    ret = -EIO;
    goto cleanup;
  }

  if ((size_t)(content_size + final_size) != size) {
    // technically, this should be impossible
    ret = -EIO;
  }

cleanup:
  EVP_CIPHER_CTX_free(d_ctx);
  return ret;
}

int aes_encrypt_auth(const void* key, const void* iv, const void* plain,
                     void* enc, size_t size, void* tag, size_t tag_size) {
  EVP_CIPHER_CTX* e_ctx;
  int ret =
      init_cipher_ctx(EVP_aes_128_gcm(), key, iv, EVP_EncryptInit_ex, &e_ctx);
  if (ret < 0) {
    return ret;
  }

  int content_size = 0;
  int final_size = 0;

  if (!EVP_EncryptUpdate(e_ctx, (uint8_t*) enc, &content_size,
                         (const uint8_t*) plain, size)) {
    ret = -EIO;
    goto cleanup;
  }

  if (!EVP_EncryptFinal_ex(e_ctx, (uint8_t*) enc + content_size, &final_size)) {
    ret = -EIO;
    goto cleanup;
  }

  if ((size_t)(content_size + final_size) != size) {
    // technically, this should be impossible
    ret = -EIO;
    goto cleanup;
  }

  if (!EVP_CIPHER_CTX_ctrl(e_ctx, EVP_CTRL_GCM_GET_TAG, tag_size, tag)) {
    ret = -EIO;
  }

cleanup:
  EVP_CIPHER_CTX_free(e_ctx);
  return ret;
}

int aes_decrypt_auth(const void* key, const void* iv, const void* enc,
                     void* plain, size_t size, const void* tag,
                     size_t tag_size) {
  EVP_CIPHER_CTX* d_ctx;
  int ret =
      init_cipher_ctx(EVP_aes_128_gcm(), key, iv, EVP_DecryptInit_ex, &d_ctx);
  if (ret < 0) {
    return ret;
  }

  int content_size = 0;
  int final_size = 0;

  if (!EVP_DecryptUpdate(d_ctx, (uint8_t*) plain, &content_size,
                         (const uint8_t*) enc, size)) {
    ret = -EIO;
    goto cleanup;
  }

  if (!EVP_CIPHER_CTX_ctrl(d_ctx, EVP_CTRL_GCM_SET_TAG, tag_size,
                           (void*) tag)) {
    ret = -EIO;
    goto cleanup;
  }

  if (EVP_DecryptFinal_ex(d_ctx, (uint8_t*) plain + content_size,
                          &final_size) <= 0) {
    // authentication failed
    ret = -EBADMSG;
    goto cleanup;
  }

  if ((size_t)(content_size + final_size) != size) {
    // technically, this should be impossible
    ret = -EIO;
  }

cleanup:
  EVP_CIPHER_CTX_free(d_ctx);
  return ret;
}
