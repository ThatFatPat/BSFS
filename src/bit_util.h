#ifndef BS_BIT_UTIL_H
#define BS_BIT_UTIL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

uint32_t read_big_endian(const void* buf);
void write_big_endian(void* buf, uint32_t host_endian);

bool get_bit(const void* buf, size_t bit);
void set_bit(void* buf, size_t bit, bool val);

#endif // BS_BIT_UTIL_H
