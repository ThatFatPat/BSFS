#include "vector.h"
#include <errno.h>
#include <stdint.h>

static int count_bits(uint8_t a) {
  int ret = 0;
  while (a) {
    ret++;
    a &= a - 1; // move to next bit
  }
  return ret;
}

/**
 * Computing the scalar product of a and b, with "size" bytes length.
 */
bool scalar_product(const uint8_t* a, const uint8_t* b, size_t size) {
  bool ret = 0;
  for (size_t i = 0; i < size; i++) {
    ret ^= count_bits(a[i] & b[i]) & 1;
  }
  return ret;
}

/**
 * Computing the norm of a, with "size" bytes length.
 */
bool norm(uint8_t* a, size_t size) {
  return scalar_product(a, a, size);
}

/**
 * Calculate the first_vector + coefficient * second_vector.
 * Put the result into linear_combination.
 */
int vector_linear_combination(void* linear_combination, void* first_vector,
                              void* second_vector, size_t vector_size,
                              bool coefficient) {

  uint8_t* int_linear_combination = (uint8_t*) linear_combination;
  uint8_t* int_first_vector = (uint8_t*) first_vector;
  uint8_t* int_second_vector = (uint8_t*) second_vector;

  for (size_t i = 0; i < vector_size; i++) {
    int_linear_combination[i] = int_first_vector[i];
    if (coefficient) {
      int_linear_combination[i] ^= int_second_vector[i];
    }
  }

  return 0;
}
