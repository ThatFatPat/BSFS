#include "vector.h"
#include "bit_util.h"
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <limits.h>
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

static matrix_t matrix_create(size_t dim);

static bool matrix_get(const_matrix_t mat, size_t row, size_t col, size_t dim);
static void matrix_set(matrix_t mat, size_t i, size_t j, bool value, size_t dim);

static int matrix_multiply3(matrix_t restrict dest, const_matrix_t a, const_matrix_t b, const_matrix_t c, size_t dim);
static void matrix_add_row(matrix_t mat, size_t row, size_t col, size_t dim);

static void matrix_gen_LUP(matrix_t L, matrix_t U, matrix_t P, size_t dim);
static void matrix_inverse_triangular(matrix_t dest, const_matrix_t triangular, bool side, size_t dim);

static size_t matrix_permutation_index_by_bmp(bool bmp[], size_t index, size_t size);

static bool rand_bit();

static matrix_t matrix_create(size_t dim){
  return malloc((dim * dim + CHAR_BIT - 1) / CHAR_BIT);
}

static bool matrix_get(const_matrix_t mat, size_t row, size_t col, size_t dim){
  return get_bit(mat, row * dim + col);
}
static void matrix_set(matrix_t mat, size_t row, size_t col, bool val, size_t dim){
  set_bit(mat, row * dim + col, val);
}

void matrix_multiply(matrix_t restrict dest, const_matrix_t a, const_matrix_t b, size_t dim){

  for(size_t row = 0; row < dim; row++){
    for(size_t col = 0; col < dim; col++){
      bool value = false;
      for(size_t k = 0; k < dim; k++){
        value ^= matrix_get(a, row, k, dim) & matrix_get(b, k, col, dim);
      }
      matrix_set(dest, row, col, value, dim);
    }
  }
}

void matrix_transpose(matrix_t dest, const_matrix_t mat, size_t dim){
  for(size_t i = 0; i < dim; i++){
    for(size_t j = 0; j < i; j++){
      bool IJ = matrix_get(mat, i, j, dim);
      bool JI = matrix_get(mat, j, i, dim);

      matrix_set(dest, i, j, JI, dim);
      matrix_set(dest, j, i, IJ, dim);
    }
    matrix_set(dest, i, i, matrix_get(mat, i, i ,dim), dim);
  }
}

int matrix_gen_nonsing(matrix_t mat, matrix_t inv, size_t dim){
  matrix_t L = matrix_create(dim);
  matrix_t U = matrix_create(dim);
  matrix_t P = matrix_create(dim);
  matrix_t Linv = matrix_create(dim);
  matrix_t Uinv = matrix_create(dim);
  matrix_t Pinv = matrix_create(dim);

  if(L == NULL || U == NULL || P == NULL || Linv == NULL || Uinv == NULL || Pinv == NULL){
    return -1;
  }

  matrix_gen_LUP(L, U, P, dim);

  matrix_inverse_triangular(Linv, L, Lower, dim);
  matrix_inverse_triangular(Uinv, U, Upper, dim);
  matrix_transpose(Pinv, P, dim);

  if(matrix_multiply3(mat, L, U, P, dim) != 0 || matrix_multiply3(inv, Pinv, Uinv, Linv, dim) != 0){
    return -1;
  }

  return 0;
}

static int matrix_multiply3(matrix_t restrict dest, const_matrix_t a, const_matrix_t b, const_matrix_t c, size_t dim){
  matrix_t mid = matrix_create(dim);
  
  if(mid == NULL){
    return -1;
  }

  matrix_multiply(mid, a, b, dim);
  matrix_multiply(dest, mid, c, dim);

  free(mid);
  return 0;
}

static void matrix_inverse_triangular(matrix_t dest, const_matrix_t triangular, bool side, size_t dim){
  for(size_t i = 0; i < dim; i++){
    for(size_t j = 0; j < dim; j++){
      if(i == j){
        matrix_set(dest, i, j, 1, dim);
      } else {
        matrix_set(dest, i, j, 0, dim);
      }
    }
  }

  for(size_t i = 0; i < dim; i++){
    for(size_t j = 0; j < i; j++){
      if(side == Lower){
        if(matrix_get(triangular, i, j, dim) == 1){
          matrix_add_row(dest, i, j, dim);
        }
      } else {
        if(matrix_get(triangular, dim - i - 1, dim - j - 1, dim) == 1){
          matrix_add_row(dest, dim - i - 1, dim - j - 1, dim);
        }
      }
    }
  }
}

static void matrix_add_row(matrix_t mat, size_t to, size_t from, size_t dim){
  for(size_t i = 0; i < dim; i++){
    bool to_val = matrix_get(mat, to, i, dim);
    bool from_val = matrix_get(mat, from, i, dim);
    matrix_set(mat, to, i, to_val ^ from_val, dim);
  }
}

static void matrix_gen_LUP(matrix_t L, matrix_t U, matrix_t P, size_t dim){

  srand(time(NULL));

  bool bmp[dim];

  for(size_t i = 0; i < dim; i++){
    bmp[i] = 0;
  }

  for(size_t i = 0; i < dim; i++){
    for(size_t j = 0; j <= i; j++){
      if(i == j){
        matrix_set(L, i, j, 1, dim);
        matrix_set(U, i, j, 1, dim);
      } else {
        matrix_set(L, i, j, rand_bit(), dim);
        matrix_set(U, j, i, rand_bit(), dim);
      }
    }
    size_t pIdx = matrix_permutation_index_by_bmp(bmp, rand() % (dim - i), dim);

    bmp[pIdx] = 1;
    matrix_set(P, i, pIdx, 1, dim);
  }
}

static size_t matrix_permutation_index_by_bmp(bool bmp[], size_t index, size_t size){
  size_t count = 0;
  for(size_t idx = 0; idx < size; idx++){
    if(bmp[idx] == 0){
      if(index == count){
        return idx;
      }
      count++;
    }
  }
  return -1;
}

static bool rand_bit(){
  return random() % 2;
}

