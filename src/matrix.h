#ifndef BS_MATRIX_H
#define BS_MATRIX_H

#include "vector.h"
#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>

typedef vector_t matrix_t;
typedef const_vector_t const_matrix_t;

void matrix_transpose(matrix_t dest, const_matrix_t matrix, size_t dim);
void matrix_multiply(matrix_t restrict dest, const_matrix_t a, const_matrix_t b,
                     size_t dim);

int matrix_gen_nonsing(matrix_t mat, size_t dim);

#endif // BS_MATRIX_H
