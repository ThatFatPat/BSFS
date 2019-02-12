#ifndef BS_VECTOR_H
#define BS_VECTOR_H

#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>

typedef uint8_t* vector_t;
typedef const uint8_t* const_vector_t;

bool vector_scalar_product(const_vector_t a, const_vector_t b, size_t size);

bool vector_norm(const_vector_t a, size_t size);

void vector_linear_combination(vector_t linear_combination,
                               const_vector_t first_vector,
                               const_vector_t second_vector, size_t vector_size,
                               bool coefficient);

int gen_nonzero_vector(vector_t vector, size_t size);

#endif // BS_VECTOR_H
