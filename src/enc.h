#ifndef BS_ENC_H
#define BS_ENC_H

#include <stddef.h>

int aes_encrypt(const void* password, size_t password_size, const void* plain,
                void* enc, size_t size);
int aes_decrypt(const void* password, size_t password_size, const void* enc,
                void* plain, size_t size);

int aes_encrypt_auth(const void* password, size_t password_size,
                     const void* plain, void* enc, size_t size, void* tag,
                     size_t tag_size);
int aes_decrypt_auth(const void* password, size_t password_size,
                     const void* enc, void* plain, size_t size, const void* tag,
                     size_t tag_size);

#endif // BS_ENC_H
