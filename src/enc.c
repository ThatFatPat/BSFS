#include "enc.h"

#include <errno.h>
#include <openssl/aes.h>
#include <openssl/evp.h>
#include <stdint.h>
#include <string.h>

#define NROUNDS 10000

static int gen_key(const EVP_CIPHER* cipher, const void* password,
                   size_t password_size, void* key, void* iv) {
  uint8_t key_iv[EVP_MAX_KEY_LENGTH + EVP_MAX_IV_LENGTH];

  int keylen = EVP_CIPHER_key_length(cipher);
  int ivlen = EVP_CIPHER_iv_length(cipher);

  if (!PKCS5_PBKDF2_HMAC((const char*) password, password_size, NULL, 0,
                         NROUNDS, EVP_sha256(), keylen + ivlen, key_iv)) {
    return -EIO;
  }

  memcpy(key, key_iv, keylen);
  memcpy(iv, key_iv + keylen, ivlen);

  return 0;
}

int aes_encrypt(const void* password, size_t password_size, const void* plain,
                void* enc, size_t size) {
  if (size % AES_BLOCK_SIZE != 0) {
    return -EINVAL;
  }

  int ret = 0;
  int content_size, final_size;
  EVP_CIPHER_CTX* e_ctx = EVP_CIPHER_CTX_new();

  if (!e_ctx) {
    return -ENOMEM;
  }

  uint8_t key[16], iv[16];
  ret = gen_key(EVP_aes_128_cbc(), password, password_size, key, iv);

  if (ret < 0) {
    goto cleanup;
  }

  if (!EVP_EncryptInit_ex(e_ctx, EVP_aes_128_cbc(), NULL, key, iv)) {
    ret = -EIO;
    goto cleanup;
  }
  EVP_CIPHER_CTX_set_padding(e_ctx, 0);

  content_size = 0;
  final_size = 0;

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
    ret = -EIO;
  }

cleanup:
  EVP_CIPHER_CTX_free(e_ctx);
  return ret;
}

int aes_decrypt(const void* password, size_t password_size, const void* enc,
                void* plain, size_t size) {
  if (size % AES_BLOCK_SIZE != 0) {
    return -EINVAL;
  }

  int ret = 0;
  int content_size, final_size;
  EVP_CIPHER_CTX* d_ctx = EVP_CIPHER_CTX_new();

  if (!d_ctx) {
    return -ENOMEM;
  }

  uint8_t key[16], iv[16];
  ret = gen_key(EVP_aes_128_cbc(), password, password_size, key, iv);

  if (ret < 0) {
    goto cleanup;
  }

  if (!EVP_DecryptInit_ex(d_ctx, EVP_aes_128_cbc(), NULL, key, iv)) {
    ret = -EIO;
    goto cleanup;
  }
  EVP_CIPHER_CTX_set_padding(d_ctx, 0);

  content_size = 0;
  final_size = 0;

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
    ret = -EIO;
  }

cleanup:
  EVP_CIPHER_CTX_free(d_ctx);
  return ret;
}
