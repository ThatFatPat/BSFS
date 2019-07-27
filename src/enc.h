#ifndef BS_ENC_H
#define BS_ENC_H

#include <stddef.h>

#define ENC_KEY_SIZE 16
#define ENC_IV_SIZE 16
#define ENC_BLOCK_SIZE 16

#define ENC_AUTH_KEY_SIZE 16
#define ENC_AUTH_IV_SIZE 12

int enc_rand_bytes(void* dest, size_t size);

int enc_key_from_bytes(const void* password, size_t password_size,
                       const void* salt, size_t salt_size, int iter,
                       size_t key_size, void* key);

int aes_encrypt(const void* key, const void* iv, const void* plain, void* enc,
                size_t size);
int aes_decrypt(const void* key, const void* iv, const void* enc, void* plain,
                size_t size);

int aes_encrypt_auth(const void* key, const void* iv, const void* plain,
                     void* enc, size_t size, void* tag, size_t tag_size);
int aes_decrypt_auth(const void* key, const void* iv, const void* enc,
                     void* plain, size_t size, const void* tag,
                     size_t tag_size);

#endif // BS_ENC_H
