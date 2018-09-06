#include "test_enc.h"
#include "enc.h"
#include <stdlib.h>
#include <string.h>


START_TEST(test_roundtrip1)
{
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

  ck_assert_str_eq(plain, (char*)decrypted);

  free(decrypted);
  free(cipher);
}
END_TEST

Suite* enc_suite(void) {
  Suite* suite = suite_create("enc");
  TCase* roundtrip_tcase = tcase_create("Round Trip");

  tcase_add_test(roundtrip_tcase, test_roundtrip1);
  suite_add_tcase(suite, roundtrip_tcase);

  return suite;
}
