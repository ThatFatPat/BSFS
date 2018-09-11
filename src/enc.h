#ifndef BS_ENC_H
#define BS_ENC_H

#include <stddef.h>

int aes_encrypt(const void* password, size_t password_size, const void* data,
                size_t size, void** buf_pointer, size_t* buf_size);
int aes_decrypt(const void* password, size_t password_size, const void* enc,
                size_t size, void** buf_pointer, size_t* buf_size);

#endif // BS_ENC_H
