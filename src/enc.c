#include "enc.h"

#include <errno.h>
#include <openssl/aes.h>
#include <openssl/evp.h>
#include <stdint.h>

#define NROUNDS 5

static int gen_key(const void* password, size_t password_size, void* key,
                   void* iv) {
  int res_size =
      EVP_BytesToKey(EVP_aes_128_cbc(), EVP_sha1(), NULL, (uint8_t*) password,
                     password_size, NROUNDS, (uint8_t*) key, (uint8_t*) iv);
  if (res_size != 16) {
    return -EIO;
  }
  return 0;
}

int aes_encrypt(const void* password, size_t password_size, const void* data,
                size_t size, void** buf_pointer, size_t* buf_size) {
  int ret = 0;
  int content_size, final_size;
  uint8_t* ciphertext;
  EVP_CIPHER_CTX* e_ctx = EVP_CIPHER_CTX_new();

  if (!e_ctx) {
    return -ENOMEM;
  }

  uint8_t key[16], iv[16];
  ret = gen_key(password, password_size, key, iv);

  if (ret != 0) {
    goto cleanup_ctx;
  }

  if (!EVP_EncryptInit_ex(e_ctx, EVP_aes_128_cbc(), NULL, key, iv)) {
    ret = -EIO;
    goto cleanup_ctx;
  }

  content_size = 0;
  final_size = 0;

  ciphertext = (uint8_t*) malloc(size + AES_BLOCK_SIZE);
  if (!ciphertext) {
    ret = -ENOMEM;
    goto cleanup_ctx;
  }

  if (!EVP_EncryptUpdate(e_ctx, ciphertext, &content_size,
                         (const uint8_t*) data, size)) {
    ret = -EIO;
    free(ciphertext);
    goto cleanup_ctx;
  }

  if (!EVP_EncryptFinal_ex(e_ctx, ciphertext + content_size, &final_size)) {
    ret = -EIO;
    free(ciphertext);
    goto cleanup_ctx;
  }

  *buf_size = content_size + final_size;
  *buf_pointer = ciphertext;

cleanup_ctx:
  EVP_CIPHER_CTX_free(e_ctx);

  return ret;
}

int aes_decrypt(const void* password, size_t password_size, const void* enc,
                size_t size, void** buf_pointer, size_t* buf_size) {
  int ret = 0;
  int content_size, final_size;
  EVP_CIPHER_CTX* d_ctx = EVP_CIPHER_CTX_new();

  if (!d_ctx) {
    return -ENOMEM;
  }

  uint8_t key[16], iv[16];
  ret = gen_key(password, password_size, key, iv);

  if (ret != 0) {
    goto cleanup_ctx;
  }

  if (!EVP_DecryptInit_ex(d_ctx, EVP_aes_128_cbc(), NULL, key, iv)) {
    ret = -EIO;
    goto cleanup_ctx;
  }

  content_size = 0;
  final_size = 0;
  uint8_t* plaintext = (uint8_t*) malloc(size);
  if (!plaintext) {
    ret = -ENOMEM;
    goto cleanup_ctx;
  }

  if (!EVP_DecryptUpdate(d_ctx, plaintext, &content_size, (const uint8_t*) enc,
                         size)) {
    ret = -EIO;
    free(plaintext);
    goto cleanup_ctx;
  }

  if (!EVP_DecryptFinal_ex(d_ctx, plaintext + content_size, &final_size)) {
    ret = -EIO;
    free(plaintext);
    goto cleanup_ctx;
  }

  *buf_size = content_size + final_size;
  *buf_pointer = plaintext;

cleanup_ctx:
  EVP_CIPHER_CTX_free(d_ctx);

  return ret;
}
