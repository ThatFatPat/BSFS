#include "test_stego.h"
#include "disk.h"
#include "keytab.h"
#include "stego.h"
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <unistd.h>

static int count_bits(uint8_t a) {
  int ret = 0;
  while (a) {
    ret++;
    a &= a - 1; // move to next bit
  }
  return ret;
}

/**
 * Computing the scalar product of a and b, with "size" bytes length.
 */
static bool scalar_product(const uint8_t* a, const uint8_t* b, size_t size) {
  bool ret = 0;
  for (size_t i = 0; i < size; i++) {
    ret ^= count_bits(a[i] & b[i]) & 1;
  }
  return ret;
}

/**
 * Computing the norm of a, with "size" bytes length.
 */
static bool norm(uint8_t* a, size_t size) {
  return scalar_product(a, a, size);
}

/**
 * Don't check for linear independency
 */
static bool check_orthonormality(void* buf, int count) {
  uint8_t* int_buf = (uint8_t*) buf;
  size_t key_size = COVER_FILE_COUNT / CHAR_BIT;
  size_t total_keys_size = count * (key_size);
  for (size_t i = 0; i < total_keys_size; i += key_size) {
    uint8_t* key_1 = int_buf + i;
    if (norm(key_1, key_size) == 0) {
      return false;
    }
    for (size_t j = 0; j < i; j += key_size) {
      uint8_t* key_2 = int_buf + j;
      if (scalar_product(key_1, key_2, key_size) == 1) {
        return false;
      }
    }
  }
  return true;
}

START_TEST(test_gen_keys) {
  uint8_t buf[16 * (COVER_FILE_COUNT / CHAR_BIT)];
  for (int count = 12; count <= 16; count++) { // Check for 10 to 16 keys
    for (int i = 0; i < 15; i++) {
      stego_gen_keys(buf, count);
      ck_assert_msg(check_orthonormality(buf, count),
                    "Error in stego_gen_keys");
    }
  }
}
END_TEST

/*-----------------------------------------------------------------------------------*/

#define KEYTAB_SIZE (KEYTAB_ENTRY_SIZE * MAX_LEVELS)

static int vector_linear_combination(void* linear_combination,
                                     void* first_vector, void* second_vector,
                                     size_t vector_size, bool coefficient) {

  uint8_t* int_linear_combination = (uint8_t*) linear_combination;
  uint8_t* int_first_vector = (uint8_t*) first_vector;
  uint8_t* int_second_vector = (uint8_t*) second_vector;

  for (size_t i = 0; i < vector_size; i++) {
    int_linear_combination[i] = int_first_vector[i];
    if (coefficient) {
      int_linear_combination[i] ^= int_second_vector[i];
    }
  }

  return 0;
}

static int open_tmp_file() {
  int fptr = syscall(SYS_memfd_create, "data.bsf", 0);
  char content[] = "Hello, I'm the Doctor.\n Basically, Run.\n";
  write(fptr, content, 5000);
  return fptr;
}

START_TEST(test_linear_combination) {
  int vector1 = 38262900;
  int vector2 = 93374014;
  int linear_combination1;
  int linear_combination2;
  vector_linear_combination((void*) &linear_combination1, (void*) &vector1,
                            (void*) &vector2, sizeof(int), 0);
  vector_linear_combination((void*) &linear_combination2, (void*) &vector1,
                            (void*) &vector2, sizeof(int), 1);
  ck_assert_uint_eq(linear_combination1, vector1);
  ck_assert_uint_eq(linear_combination2, vector1 ^ vector2);
}
END_TEST

/*-----------------------------------------------------------------------------------*/

Suite* stego_suite(void) {

  Suite* suite = suite_create("stego");
  TCase* stego_gen_keys_tcase = tcase_create("stego_gen_keys");
  TCase* stego_read_write_tcase = tcase_create("stego_read_write");

  tcase_add_test(stego_gen_keys_tcase, test_gen_keys);
  suite_add_tcase(suite, stego_gen_keys_tcase);

  tcase_add_test(stego_read_write_tcase, test_linear_combination);
  suite_add_tcase(suite, stego_read_write_tcase);

  return suite;
}
