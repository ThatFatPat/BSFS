#include "test_enc.h"

#include "enc.h"
#include <stdlib.h>
#include <string.h>

START_TEST(test_roundtrip) {
  const char* password = "Doctor Who";
  const char* plain = "But what kind of Doctor?";

  void* cipher = NULL;
  size_t cipher_size = 0;
  int status = aes_encrypt(password, strlen(password), plain, strlen(plain) + 1,
                           &cipher, &cipher_size);
  ck_assert_int_eq(status, 0);

  void* decrypted = NULL;
  size_t decrypted_size = 0;
  status = aes_decrypt(password, strlen(password), cipher, cipher_size,
                       &decrypted, &decrypted_size);
  ck_assert_int_eq(status, 0);

  ck_assert_str_eq(plain, (char*) decrypted);

  free(decrypted);
  free(cipher);
}
END_TEST

START_TEST(test_roundup_buffer) {
  const char* password = "Doctor Who";
  const char* plain = "But what kind of Doctor?";

  void* cipher = NULL;
  size_t cipher_size = 0;
  int status = aes_encrypt(password, strlen(password), plain, strlen(plain) + 1,
                           &cipher, &cipher_size);
  ck_assert_int_eq(status, 0);
  ck_assert_int_eq(cipher_size % 16, 0);
  ck_assert_int_ne(cipher_size, strlen(plain));
  ck_assert_int_ne(cipher_size, 0);

  free(cipher);
}
END_TEST

START_TEST(test_get_encrypted_size) {
  ck_assert_int_eq(aes_get_encrypted_size(8), 16);
  ck_assert_int_eq(aes_get_encrypted_size(16), 32);
}
END_TEST

Suite* enc_suite(void) {
  Suite* suite = suite_create("enc");

  TCase* roundtrip_tcase = tcase_create("roundtrip");
  tcase_add_test(roundtrip_tcase, test_roundtrip);
  suite_add_tcase(suite, roundtrip_tcase);

  TCase* size_tcase = tcase_create("sizes");
  tcase_add_test(size_tcase, test_roundup_buffer);
  tcase_add_test(size_tcase, test_get_encrypted_size);
  suite_add_tcase(suite, size_tcase);

  return suite;
}
