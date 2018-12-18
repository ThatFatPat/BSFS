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

// Matrix implementation

static size_t round_to_bytes(size_t bits) {
  return (bits + CHAR_BIT - 1) / CHAR_BIT;
}

static matrix_t matrix_create(size_t dim) {
  return calloc(1, round_to_bytes(dim * dim));
}

static bool matrix_get(const_matrix_t mat, size_t row, size_t col, size_t dim) {
  return get_bit(mat, row * dim + col);
}

static void matrix_set(matrix_t mat, size_t row, size_t col, bool val,
                       size_t dim) {
  set_bit(mat, row * dim + col, val);
}

static void matrix_add_row(matrix_t mat, size_t to, size_t from, size_t dim) {
  for (size_t i = 0; i < dim; i++) {
    bool to_val = matrix_get(mat, to, i, dim);
    bool from_val = matrix_get(mat, from, i, dim);
    matrix_set(mat, to, i, to_val ^ from_val, dim);
  }
}

static void matrix_inverse_triangular(matrix_t dest, const_matrix_t triangular,
                                      bool lower_triangular, size_t dim) {
  // initialize to identity
  for (size_t i = 0; i < dim; i++) {
    for (size_t j = 0; j < dim; j++) {
      matrix_set(dest, i, j, i == j, dim);
    }
  }

  // perform row operations
  for (size_t i = 0; i < dim; i++) {
    for (size_t j = 0; j < i; j++) {
      if (lower_triangular) {
        if (matrix_get(triangular, i, j, dim)) {
          matrix_add_row(dest, i, j, dim);
        }
      } else {
        if (matrix_get(triangular, dim - i - 1, dim - j - 1, dim)) {
          matrix_add_row(dest, dim - i - 1, dim - j - 1, dim);
        }
      }
    }
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

static int matrix_gen_LUP(matrix_t L, matrix_t U, matrix_t P, size_t dim) {
  void* bmp = calloc(1, round_to_bytes(dim));
  if (!bmp) {
    return -ENOMEM;
  }

  int ret = 0;

  for (size_t i = 0; i < dim; i++) {
    for (size_t j = 0; j <= i; j++) {
      if (i == j) {
        matrix_set(L, i, j, 1, dim);
        matrix_set(U, i, j, 1, dim);
      } else {
        bool bit;

        ret = rand_bit(&bit);
        if (ret < 0) {
          goto cleanup;
        }
        matrix_set(L, i, j, bit, dim);

        ret = rand_bit(&bit);
        if (ret < 0) {
          goto cleanup;
        }
        matrix_set(U, j, i, bit, dim);
      }
    }

    size_t bmp_idx;
    ret = rand_index(dim - i, &bmp_idx);
    if (ret < 0) {
      goto cleanup;
    }

    size_t perm_idx = nth_free_space(bmp, bmp_idx, dim);
    set_bit(bmp, perm_idx, 1);
    matrix_set(P, i, perm_idx, 1, dim);
  }

cleanup:
  free(bmp);
  return ret;
}

static int matrix_multiply3(matrix_t restrict dest, const_matrix_t a,
                            const_matrix_t b, const_matrix_t c, size_t dim) {
  matrix_t mid = matrix_create(dim);

  if (!mid) {
    return -ENOMEM;
  }

  matrix_multiply(mid, a, b, dim);
  matrix_multiply(dest, mid, c, dim);

  free(mid);
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

void matrix_transpose(matrix_t dest, const_matrix_t mat, size_t dim) {
  for (size_t i = 0; i < dim; i++) {
    for (size_t j = 0; j < i; j++) {
      bool IJ = matrix_get(mat, i, j, dim);
      bool JI = matrix_get(mat, j, i, dim);

      matrix_set(dest, i, j, JI, dim);
      matrix_set(dest, j, i, IJ, dim);
    }
    matrix_set(dest, i, i, matrix_get(mat, i, i, dim), dim);
  }
}

int matrix_gen_nonsing(matrix_t mat, matrix_t inv, size_t dim) {
  size_t matrix_storage_size = round_to_bytes(dim * dim);

  // Allocate space for: L, U, P, Linv, Uinv, Pinv, temp
  matrix_t storage = (matrix_t) calloc(7, matrix_storage_size);
  if (!storage) {
    return -ENOMEM;
  }

  matrix_t L = storage;
  matrix_t U = L + matrix_storage_size;
  matrix_t P = U + matrix_storage_size;
  matrix_t Linv = P + matrix_storage_size;
  matrix_t Uinv = Linv + matrix_storage_size;
  matrix_t Pinv = Uinv + matrix_storage_size;
  matrix_t temp = Pinv + matrix_storage_size;

  int ret = 0;

  ret = matrix_gen_LUP(L, U, P, dim);
  if (ret < 0) {
    goto cleanup;
  }

  matrix_inverse_triangular(Linv, L, true, dim);
  matrix_inverse_triangular(Uinv, U, false, dim);
  matrix_transpose(Pinv, P, dim);

  // mat = L*U*P
  matrix_multiply(temp, L, U, dim);
  matrix_multiply(mat, temp, P, dim);

  // inv = Pinv*Uinv*Linv
  matrix_multiply(temp, Pinv, Uinv, dim);
  matrix_multiply(inv, temp, Linv, dim);

cleanup:
  free(storage);
  return ret;
}
