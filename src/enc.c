#include "enc.h"
#include <openssl/aes.h>
#include <openssl/evp.h>

#define NROUNDS 5

static int gen_key(const void* password, size_t password_size, void* key, void* iv) {
 	int res_size;
 	key = malloc(sizeof(unsigned char) * 16);
	iv = malloc(sizeof(unsigned char) * 16);
 	/*
   * Gen key & IV for AES 128 CBC mode. A SHA1 digest is used to hash the supplied key material.
   * nrounds is the number of times the we hash the material. More rounds are more secure but
   * slower.
   */
  res_size = EVP_BytesToKey(EVP_aes_128_cbc(), EVP_sha1(), NULL, password, password_size, NROUNDS, key, iv);
  if (res_size != 16) {
    return -1;
  }
  return 0;
}

int aes_encrypt(const void* password, size_t password_size, const void* data, size_t size, void** buf_pointer, size_t* buf_size) {
  return 0;
}

int aes_decrypt(const void* password, size_t password_size, const void* enc, size_t size, void** buf_pointer, size_t* buf_size) {
  return 0;
}