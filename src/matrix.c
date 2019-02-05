#include "matrix.h"

#include "bit_util.h"
#include "vector.h"
#include <errno.h>
#include <limits.h>
#include <openssl/rand.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>

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

/**
 * Iterative algorithm that creates a random nonsingular matrix
 */
int matrix_gen_nonsing(matrix_t mat, size_t dim) {
  size_t matrix_storage_size = round_to_bytes(dim * dim);

  int ret = 0;

  for (size_t i = 0; i < dim; i++) {
    for (size_t j = 0; j < dim; j++) {
      matrix_set(mat, i, j, 0, dim);
    }
  }

  uint8_t* bmp = (uint8_t*) calloc(1, round_to_bytes(dim)); // Bitmap of minors
  vector_t b = (vector_t) malloc(round_to_bytes(dim));      // A random vector
  vector_t c =
      (vector_t) malloc(round_to_bytes(dim)); // A random non-zero vector

  if (!bmp || !c || !b) {
    ret = -ENOMEM;
    goto cleanup;
  }

  for (size_t i = 0; i < dim - 1; i++) {
    size_t size = dim - i;

    if (!gen_nonzero_vector(c, size) || !RAND_bytes(b, size)) {
      ret = -EIO;
      goto cleanup;
    }

    bool saw_nonzero_in_c = false;
    size_t temp_size = size;
    size_t idx_in_minor = 0;
    size_t idx_in_mat = 0;

    while (idx_in_mat < temp_size) {
      if (!get_bit(bmp, idx_in_mat)) {

        bool c_value = get_bit(c, idx_in_minor);

        if (c_value) {
          if (!saw_nonzero_in_c) {
            saw_nonzero_in_c = true;
            set_bit(bmp, idx_in_mat, true);
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

cleanup:
  free(bmp);
  free(b);
  free(c);
  return ret;
}
