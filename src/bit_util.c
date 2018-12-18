#include "bit_util.h"

#include <arpa/inet.h>
#include <limits.h>
#include <string.h>

uint32_t read_big_endian(const void* buf) {
  uint32_t big_endian;
  memcpy(&big_endian, buf, sizeof(uint32_t));
  return ntohl(big_endian);
}

void write_big_endian(void* buf, uint32_t host_endian) {
  uint32_t big_endian = htonl(host_endian);
  memcpy(buf, &big_endian, sizeof(uint32_t));
}

static size_t byte_from_bit(size_t bit) {
  return bit / CHAR_BIT;
}

static size_t bit_rem_from_bit(size_t bit) {
  return CHAR_BIT - 1 - bit % CHAR_BIT;
}

bool get_bit(const void* buf, size_t bit) {
  const uint8_t* byte = (const uint8_t*) buf + byte_from_bit(bit);
  return (*byte >> bit_rem_from_bit(bit)) & 1;
}

void set_bit(void* buf, size_t bit, bool val) {
  uint8_t* byte = (uint8_t*) buf + byte_from_bit(bit);
  if (val) {
    *byte |= (uint8_t) 1 << bit_rem_from_bit(bit);
  } else {
    *byte &= ~((uint8_t) 1 << bit_rem_from_bit(bit));
  }
}

size_t round_to_bytes(size_t bits) {
  return (bits + CHAR_BIT - 1) / CHAR_BIT;
}
