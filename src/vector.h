#ifndef BS_VECTOR_H
#define BS_VECTOR_H

#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>

typedef uint8_t* vector_t;
typedef const uint8_t* const_vector_t;

typedef vector_t matrix_t;
typedef const_vector_t const_matrix_t;

// Vector API

bool vector_scalar_product(const_vector_t a, const_vector_t b, size_t size);

bool vector_norm(const_vector_t a, size_t size);

void vector_linear_combination(vector_t linear_combination,
                               const_vector_t first_vector,
                               const_vector_t second_vector, size_t vector_size,
                               bool coefficient);

// Matrix API

void matrix_multiply(matrix_t restrict dest, const_matrix_t a, const_matrix_t b,
                     size_t dim);

int matrix_gen_nonsing(matrix_t mat, size_t dim);

#endif // BS_VECTOR_H
