#include "test_enc.h"

#include "enc.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>

START_TEST(test_roundtrip) {
  const char* password = "Doctor Who";
  const char plain[32] = "But what kind of Doctor?";

  char cipher[sizeof(plain)];
  char decrypted[sizeof(plain)];

  int status =
      aes_encrypt(password, strlen(password), plain, cipher, sizeof(plain));
  ck_assert_int_eq(status, 0);

  status = aes_decrypt(password, strlen(password), cipher, decrypted,
                       sizeof(cipher));
  ck_assert_int_eq(status, 0);

  ck_assert_str_eq(plain, (char*) decrypted);
}
END_TEST

START_TEST(test_encrypt_wrong_size) {
  const char* password = "password1";
  const char plain[] = "Hello, this plaintext is the wrong size!!!";

  char cipher[sizeof(plain)];

  ck_assert_int_eq(
      aes_encrypt(password, strlen(password), plain, cipher, sizeof(plain)),
      -EINVAL);
}
END_TEST

Suite* enc_suite(void) {
  Suite* suite = suite_create("enc");

  TCase* roundtrip_tcase = tcase_create("roundtrip");
  tcase_add_test(roundtrip_tcase, test_roundtrip);
  suite_add_tcase(suite, roundtrip_tcase);

  TCase* size_tcase = tcase_create("size");
  tcase_add_test(size_tcase, test_encrypt_wrong_size);
  suite_add_tcase(suite, size_tcase);

  return suite;
}
