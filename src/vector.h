#ifndef BS_VECTOR_H
#define BS_VECTOR_H

#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>

int count_bits(uint8_t a);

bool scalar_product(const uint8_t* a, const uint8_t* b, size_t size);

bool norm(uint8_t* a, size_t size);

void vector_linear_combination(void* linear_combination, void* first_vector,
                               void* second_vector, size_t vector_size,
                               bool coefficient);

#endif // BS_VECTOR_H