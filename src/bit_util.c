#include "bit_util.h"

#include <arpa/inet.h>
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

bool get_bit(const void* buf, size_t bit) {
  const uint8_t* buf_bytes = (const uint8_t*) buf;
  return (buf_bytes[bit / 8] >> (7 - bit % 8)) & 1;
}

void set_bit(void* buf, size_t bit, bool val) {
  uint8_t* byte = (uint8_t*) buf + bit / 8;
  if (val) {
    *byte |= (uint8_t) 1 << (7 - bit % 8);
  } else {
    *byte &= ~((uint8_t) 1 << (7 - bit % 8));
  }
}
