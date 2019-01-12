#include "test_enc.h"

#include "enc.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>

START_TEST(test_basic_roundtrip) {
  const char* password = "Doctor Who";
  const char plain[32] = "But what kind of Doctor?";

  const char salt[] = "salt!!!";
  const char bad_salt[] = "bad salt";

  char cipher[sizeof(plain)];
  char decrypted[sizeof(plain)];

  ck_assert_int_eq(aes_encrypt(password, strlen(password), salt, sizeof(salt),
                               plain, cipher, sizeof(plain)),
                   0);

  ck_assert_int_eq(aes_decrypt(password, strlen(password), salt, sizeof(salt),
                               cipher, decrypted, sizeof(cipher)),
                   0);

  ck_assert_str_eq(plain, (char*) decrypted);

  ck_assert_int_eq(aes_decrypt(password, strlen(password), bad_salt,
                               sizeof(bad_salt), cipher, decrypted,
                               sizeof(cipher)),
                   0);

  ck_assert_int_ne(memcmp(decrypted, plain, sizeof(plain)), 0);
}
END_TEST

START_TEST(test_basic_wrong_size) {
  const char* password = "password1";
  const char plain[] = "Hello, this plaintext is the wrong size!!!";

  char cipher[sizeof(plain)];

  ck_assert_int_eq(aes_encrypt(password, strlen(password), NULL, 0, plain,
                               cipher, sizeof(plain)),
                   -EINVAL);
}
END_TEST

START_TEST(test_auth_roundtrip) {
  const char* password = "pass2";
  const char salt[] = "salt!!!";

  const char plain[] = "This is plaintext!";

  char cipher[sizeof(plain)];
  char decrypted[sizeof(plain)];
  char tag[16];

  ck_assert_int_eq(aes_encrypt_auth(password, strlen(password), salt,
                                    sizeof(salt), plain, cipher, sizeof(plain),
                                    tag, sizeof(tag)),
                   0);

  ck_assert_int_eq(aes_decrypt_auth(password, strlen(password), salt,
                                    sizeof(salt), cipher, decrypted,
                                    sizeof(cipher), tag, sizeof(tag)),
                   0);

  ck_assert_str_eq(plain, decrypted);
}
END_TEST

START_TEST(test_auth_corrupt) {
  const char* password = "pass2";
  const char salt[] = "salt!!!";

  const char plain[] = "This is plaintext!";

  char cipher[sizeof(plain)];
  char decrypted[sizeof(plain)];
  char tag[16];

  ck_assert_int_eq(aes_encrypt_auth(password, strlen(password), salt,
                                    sizeof(salt), plain, cipher, sizeof(plain),
                                    tag, sizeof(tag)),
                   0);

  memcpy(cipher, "blabla", 6);

  ck_assert_int_eq(aes_decrypt_auth(password, strlen(password), salt,
                                    sizeof(salt), cipher, decrypted,
                                    sizeof(cipher), tag, sizeof(tag)),
                   -EBADMSG);
}
END_TEST

Suite* enc_suite(void) {
  Suite* suite = suite_create("enc");

  TCase* basic_tcase = tcase_create("basic");
  tcase_add_test(basic_tcase, test_basic_roundtrip);
  tcase_add_test(basic_tcase, test_basic_wrong_size);
  suite_add_tcase(suite, basic_tcase);

  TCase* auth_tcase = tcase_create("auth");
  tcase_add_test(auth_tcase, test_auth_roundtrip);
  tcase_add_test(auth_tcase, test_auth_corrupt);
  suite_add_tcase(suite, auth_tcase);

  return suite;
}
