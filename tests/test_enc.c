#include "test_enc.h"

#include "enc.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>

START_TEST(test_key_from_bytes) {
  const char* password = "password1";
  const char* salt = "salt!!!";

  uint8_t key[16];

  ck_assert_int_eq(enc_key_from_bytes(password, strlen(password), salt,
                                      strlen(salt), 5, sizeof(key), key),
                   0);
}
END_TEST

START_TEST(test_basic_roundtrip) {
  const char key[ENC_KEY_SIZE] = "abcdefg1293";
  const char plain[32] = "But what kind of Doctor?";

  const char iv[ENC_IV_SIZE] = "iv!!!";
  const char bad_iv[ENC_IV_SIZE] = "bad iv!!!";

  char cipher[sizeof(plain)];
  char decrypted[sizeof(plain)];

  ck_assert_int_eq(aes_encrypt(key, iv, plain, cipher, sizeof(plain)), 0);

  ck_assert_int_eq(aes_decrypt(key, iv, cipher, decrypted, sizeof(cipher)), 0);

  ck_assert_str_eq(plain, (char*) decrypted);

  ck_assert_int_eq(aes_decrypt(key, bad_iv, cipher, decrypted, sizeof(cipher)),
                   0);

  ck_assert_int_ne(memcmp(decrypted, plain, sizeof(plain)), 0);
}
END_TEST

START_TEST(test_basic_wrong_size) {
  const char key[ENC_KEY_SIZE] = "key";
  const char iv[ENC_IV_SIZE] = "iv";

  const char plain[] = "Hello, this plaintext is the wrong size!!!";
  char cipher[sizeof(plain)];

  ck_assert_int_eq(aes_encrypt(key, iv, plain, cipher, sizeof(plain)), -EINVAL);
}
END_TEST

START_TEST(test_auth_roundtrip) {
  const char key[ENC_AUTH_KEY_SIZE] = "ajklsdjfaskassdf";
  const char iv[ENC_AUTH_IV_SIZE] = "asjkdfi3eqi9";

  const char plain[] = "This is plaintext!";

  char cipher[sizeof(plain)];
  char decrypted[sizeof(plain)];
  char tag[16];

  ck_assert_int_eq(
      aes_encrypt_auth(key, iv, plain, cipher, sizeof(plain), tag, sizeof(tag)),
      0);

  ck_assert_int_eq(aes_decrypt_auth(key, iv, cipher, decrypted, sizeof(cipher),
                                    tag, sizeof(tag)),
                   0);

  ck_assert_str_eq(plain, decrypted);
}
END_TEST

START_TEST(test_auth_corrupt) {
  const char key[ENC_AUTH_KEY_SIZE] = "ajklsdjfaskassdf";
  const char iv[ENC_AUTH_IV_SIZE] = "asjkdfi3eqi9";

  const char plain[] = "This is plaintext!";

  char cipher[sizeof(plain)];
  char decrypted[sizeof(plain)];
  char tag[16];

  ck_assert_int_eq(
      aes_encrypt_auth(key, iv, plain, cipher, sizeof(plain), tag, sizeof(tag)),
      0);

  memcpy(cipher, "blabla", 6);

  ck_assert_int_eq(aes_decrypt_auth(key, iv, cipher, decrypted, sizeof(cipher),
                                    tag, sizeof(tag)),
                   -EBADMSG);
}
END_TEST

Suite* enc_suite(void) {
  Suite* suite = suite_create("enc");

  TCase* pbkdf_tcase = tcase_create("pbkdf");
  tcase_add_test(pbkdf_tcase, test_key_from_bytes);
  suite_add_tcase(suite, pbkdf_tcase);

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
