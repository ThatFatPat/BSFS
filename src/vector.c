#include "vector.h"

#include "bit_util.h"
#include <errno.h>
#include <limits.h>
#include <openssl/rand.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>

// Vector implementation

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

static int gen_nonzero_vector(vector_t vector, size_t dim);

// Matrix implementation

static bool matrix_get(const_matrix_t mat, size_t row, size_t col, size_t dim) {
  return get_bit(mat, row * dim + col);
}

static void matrix_set(matrix_t mat, size_t row, size_t col, bool val,
                       size_t dim) {
  set_bit(mat, row * dim + col, val);
}

static void matrix_add(matrix_t mat, size_t row, size_t col, bool added_val,
                       size_t dim) {
  if (added_val) {
    matrix_set(mat, row, col, !matrix_get(mat, row, col, dim), dim);
  }
}

static void matrix_add_row(matrix_t mat, size_t to, size_t from, size_t dim) {
  // Note: can't use `vector_linear_combination` as the row boundaries may not
  // always fall on byte boundaries.
  for (size_t i = 0; i < dim; i++) {
    bool to_val = matrix_get(mat, to, i, dim);
    bool from_val = matrix_get(mat, from, i, dim);
    matrix_set(mat, to, i, to_val ^ from_val, dim);
  }
}

static size_t nth_free_space(void* bmp, size_t n, size_t size) {
  size_t count = 0;
  for (size_t i = 0; i < size; i++) {
    if (!get_bit(bmp, i)) {
      if (n == count) {
        return i;
      }
      count++;
    }
  }

  return -1;
}

static int rand_bit(bool* bit) {
  uint8_t byte;
  if (!RAND_bytes(&byte, 1)) {
    return -EIO;
  }
  *bit = byte & 1;
  return 0;
}

// Generate an unbiased random integer in [0, n)
static int rand_index(size_t n, size_t* out) {
  size_t threshold = SIZE_MAX - SIZE_MAX % n;

  size_t raw_rnd;
  do {
    if (!RAND_bytes((uint8_t*) &raw_rnd, sizeof(raw_rnd))) {
      return -EIO;
    }
  } while (raw_rnd >= threshold);

  *out = raw_rnd % n;
  return 0;
}

void matrix_multiply(matrix_t restrict dest, const_matrix_t a, const_matrix_t b,
                     size_t dim) {

  for (size_t row = 0; row < dim; row++) {
    for (size_t col = 0; col < dim; col++) {
      bool value = false;
      for (size_t k = 0; k < dim; k++) {
        value ^= matrix_get(a, row, k, dim) & matrix_get(b, k, col, dim);
      }
      matrix_set(dest, row, col, value, dim);
    }
  }
}

static int gen_nonzero_vector(vector_t vector, size_t dim) {

  size_t r;
  if (!RAND_bytes(vector, round_to_bytes(dim)) || !rand_index(dim, &r)) {
    return -EIO;
  }
  vector[r / CHAR_BIT] |= 1UL << (r % CHAR_BIT);
  return 0;
}

/**
 * Iterative algorithm that creates a random nonsingular matrix
 */
int matrix_gen_nonsing(matrix_t mat, size_t dim) {
  size_t matrix_storage_size = round_to_bytes(dim * dim);

  for (size_t i = 0; i < dim; i++) {
    for (size_t j = 0; j < dim; j++) {
      matrix_set(mat, i, j, 0, dim);
    }
  }

  bool bmp[dim]; // Bitmap of minors

  for (size_t i = 0; i < dim; i++) {
    bmp[i] = 0;
  }

  for (size_t i = 0; i < dim; i++) {
    bmp[i] = 0;
  }

  vector_t b = (vector_t) malloc(round_to_bytes(dim)); // A random vector
  vector_t c =
      (vector_t) malloc(round_to_bytes(dim)); // A random non-zero vector

  if (!c || !b) {
    return -ENOMEM;
  }

  for (size_t i = 0; i < dim - 1; i++) {
    size_t size = dim - i;

    if (!gen_nonzero_vector(c, size) || !RAND_bytes(b, size)) {
      return -EIO;
    }

    bool saw_nonzero_in_c = false;
    size_t temp_size = size;
    size_t idx_in_minor = 0;
    size_t idx_in_mat = 0;

    while (idx_in_mat < temp_size) {
      if (!bmp[idx_in_mat]) {

        bool c_value = get_bit(c, idx_in_minor);

        if (c_value) {
          if (!saw_nonzero_in_c) {
            saw_nonzero_in_c = true;
            bmp[idx_in_mat] = true;
          }

          matrix_add(mat, idx_in_mat, i, c_value, dim);
          for (size_t v = 0; v < size - 1; v++) {
            matrix_add(mat, idx_in_mat, v + i + 1, get_bit(b, v), dim);
          }
        }

        idx_in_minor++;
      } else {
        temp_size++;
      }
      idx_in_mat++;
    }
  }

  free(b);
  free(c);
  return 0;
}