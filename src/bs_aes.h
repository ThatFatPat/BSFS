
#ifndef BS_AES_H
#define BS_AES_H
#include "openssl/aes.h"
#include "openssl/err.h"
int init_aes();
int aes_encrypt(const void* key, const void* data, size_t size, void* buf);
int aes_decrypt(const void* key, const void* enc, size_t size, void* buf);
#endif