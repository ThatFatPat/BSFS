#include "matrix.h"

#include "bit_util.h"
#include "vector.h"
#include <errno.h>
#include <limits.h>
#include <openssl/rand.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static bool matrix_get(const_matrix_t matrix, size_t row, size_t col,
                       size_t dim) {
  return get_bit(matrix, row * dim + col);
}

static void matrix_set(matrix_t matrix, size_t row, size_t col, bool val,
                       size_t dim) {
  set_bit(matrix, row * dim + col, val);
}

static void matrix_elem_add(matrix_t matrix, size_t row, size_t col,
                            bool added_val, size_t dim) {
  if (added_val) {
    matrix_set(matrix, row, col, !matrix_get(matrix, row, col, dim), dim);
  }
}

static void matrix_add_row(matrix_t matrix, size_t to, size_t from,
                           size_t dim) {
  // Note: can't use `vector_linear_combination` as the row boundaries may not
  // always fall on byte boundaries.
  for (size_t i = 0; i < dim; i++) {
    bool to_val = matrix_get(matrix, to, i, dim);
    bool from_val = matrix_get(matrix, from, i, dim);
    matrix_set(matrix, to, i, to_val ^ from_val, dim);
  }
}

void matrix_transpose(matrix_t dest, const_matrix_t matrix, size_t dim) {
  for (size_t i = 0; i < dim; i++) {
    for (size_t j = 0; j < i; j++) {
      bool IJ = matrix_get(matrix, i, j, dim);
      bool JI = matrix_get(matrix, j, i, dim);
      matrix_set(dest, i, j, JI, dim);
      matrix_set(dest, j, i, IJ, dim);
    }
    matrix_set(dest, i, i, matrix_get(matrix, i, i, dim), dim);
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
 * a step in the gen_nonsing algorithm
 */
static void gen_nonsing_dim(matrix_t matrix, const_vector_t b, const_vector_t c,
                            size_t dim, size_t curr_dim, uint8_t* bmp) {
  size_t n = dim - curr_dim;

  size_t idx_in_minor = 0; // The index of the curret element in c
  size_t idx_in_mat =
      0; // The index corresponding to the place of the place where we have to
         // put the current element in c in the original matrix

  bool found_nonzero_bit = false; // Found a nonzero bit in c

  for (idx_in_minor = 0; idx_in_minor < n; idx_in_minor++, idx_in_mat++) {
    while (get_bit(bmp, idx_in_mat)) {
      idx_in_mat++; // pass all of the selected rows in bmp
    }

    bool c_value = get_bit(c, idx_in_minor);

    if (c_value) {
      if (!found_nonzero_bit) {
        // c_value is the first non-zero calue in c
        found_nonzero_bit = true;
        set_bit(bmp, idx_in_mat, true);
      }

      matrix_elem_add(matrix, idx_in_mat, curr_dim, c_value,
                      dim); // Add c into the matrix
      // Add b into the matrix
      for (size_t v = 0; v < n - 1; v++) {
        matrix_elem_add(matrix, idx_in_mat, curr_dim + v + 1, get_bit(b, v),
                        dim);
      }
    }
  }
}

/**
 * Iterative algorithm that creates a random nonsingular matrix
 */
int matrix_gen_nonsing(matrix_t matrix, size_t dim) {
  size_t vec_size = round_to_bytes(dim);

  int ret = 0;

  for (size_t i = 0; i < dim; i++) {
    for (size_t j = 0; j < dim; j++) {
      matrix_set(matrix, i, j, 0, dim);
    }
  }

  uint8_t* bmp = (uint8_t*) calloc(1, vec_size); // Bitmap of minors
  vector_t b = (vector_t) malloc(vec_size);      // A random vector
  vector_t c = (vector_t) malloc(vec_size);      // A random non-zero vector

  if (!bmp || !c || !b) {
    ret = -ENOMEM;
    goto cleanup;
  }

  // The iterative algorithm
  for (size_t j = 0; j < dim; j++) {
    size_t n = dim - j;

    // generating c: a nonzero coloumn vector
    ret = gen_nonzero_vector(c, n);
    if (ret < 0) {
      goto cleanup;
    }

    // generating b: a row vector
    if (!RAND_bytes(b, round_to_bytes(n - 1))) {
      ret = -EIO;
      goto cleanup;
    }

    // placing b and c in the appropriate location in the matrix
    gen_nonsing_dim(matrix, b, c, dim, j, bmp);
  }

cleanup:
  free(bmp);
  free(b);
  free(c);
  return ret;
}

static void matrix_set_identity(matrix_t matrix, size_t dim) {
  for (size_t i = 0; i < dim; i++) {
    for (size_t j = 0; j < dim; j++) {
      matrix_set(matrix, i, j, i == j, dim);
    }
  }
}

static void matrix_swap_rows(matrix_t matrix, size_t r1, size_t r2,
                             size_t dim) {
  for (size_t i = 0; i < dim; i++) {
    bool tmp = matrix_get(matrix, r1, i, dim);
    matrix_set(matrix, r1, i, matrix_get(matrix, r2, i, dim), dim);
    matrix_set(matrix, r2, i, tmp, dim);
  }
}

static ssize_t find_pivot(matrix_t matrix, size_t col, size_t dim) {
  // Note: search starts from diagonal
  for (size_t row = col; row < dim; row++) {
    if (matrix_get(matrix, row, col, dim)) {
      return row;
    }
  }

  return -1;
}

int matrix_invert(matrix_t inverse, const_matrix_t matrix, size_t dim) {
  size_t byte_size = round_to_bytes(dim * dim);
  matrix_t tmp = (matrix_t) malloc(byte_size);

  if (!tmp) {
    return -ENOMEM;
  }

  int ret = 0;

  memcpy(tmp, matrix, byte_size);

  // Initialize inverse
  matrix_set_identity(inverse, dim);

  // Gaussian elimination
  for (size_t j = 0; j < dim; j++) {
    ssize_t pivot = find_pivot(tmp, j, dim);
    if (pivot < 0) {
      // matrix is singular
      ret = -EINVAL;
      goto cleanup;
    }

    // move pivot to diagonal
    matrix_swap_rows(tmp, j, pivot, dim);
    matrix_swap_rows(inverse, j, pivot, dim);

    // zero out the rest of the column
    for (size_t i = 0; i < dim; i++) {
      if (i != j && matrix_get(tmp, i, j, dim)) {
        matrix_add_row(tmp, i, j, dim);
        matrix_add_row(inverse, i, j, dim);
      }
    }
  }

cleanup:
  free(tmp);
  return ret;
}
