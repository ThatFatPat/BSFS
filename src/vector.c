#include "vector.h"

#include "bit_util.h"
#include <errno.h>
#include <limits.h>
#include <openssl/rand.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>

static bool parity(uint8_t a) {
  return __builtin_parity(a);
}

/**
 * Computing the scalar product of a and b, with "size" bytes length.
 */
bool vector_scalar_product(const_vector_t a, const_vector_t b, size_t size) {
  bool ret = 0;
  for (size_t i = 0; i < size; i++) {
    ret ^= parity(a[i] & b[i]);
  }
  return ret;
}

/**
 * Computing the norm of a, with "size" bytes length.
 */
bool vector_norm(const_vector_t a, size_t size) {
  return vector_scalar_product(a, a, size);
}

/**
 * Calculate the first_vector + coefficient * second_vector.
 * Put the result into linear_combination.
 */
void vector_linear_combination(vector_t linear_combination,
                               const_vector_t first_vector,
                               const_vector_t second_vector, size_t vector_size,
                               bool coefficient) {

  for (size_t i = 0; i < vector_size; i++) {
    linear_combination[i] = first_vector[i];
    if (coefficient) {
      linear_combination[i] ^= second_vector[i];
    }
  }
}

static bool has_nonzero(vector_t vector, size_t dim) {
  for (size_t i = 0; i < dim; i++) {
    if (get_bit(vector, i)) {
      return true;
    }
  }
  return false;
}

int gen_nonzero_vector(vector_t vector, size_t dim) {
  size_t size = round_to_bytes(dim);

  do {
    if (!RAND_bytes(vector, size)) {
      return -EIO;
    }
  } while (!has_nonzero(vector, dim));
  return 0;
}
