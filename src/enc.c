#include "enc.h"
#include <openssl/aes.h>
#include <openssl/evp.h>

#define NROUNDS 5

static int gen_key(const void* password, size_t password_size, void* key, void* iv) {
  return 0;
}

int aes_encrypt(const void* password, size_t password_size, const void* data, size_t size, void** buf_pointer, size_t* buf_size) {
  return 0;
}

int aes_decrypt(const void* password, size_t password_size, const void* enc, size_t size, void** buf_pointer, size_t* buf_size) {
  return 0;
}