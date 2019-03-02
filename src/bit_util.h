#ifndef BS_BIT_UTIL_H
#define BS_BIT_UTIL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

uint32_t read_be32(const void* buf);
void write_be32(void* buf, uint32_t host_endian);

uint64_t read_be64(const void* buf);
void write_be64(void* buf, uint64_t host_endian);

bool get_bit(const void* buf, size_t bit);
void set_bit(void* buf, size_t bit, bool val);

size_t round_to_bytes(size_t bits);

#endif // BS_BIT_UTIL_H
