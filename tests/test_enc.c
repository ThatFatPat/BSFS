#include "test_enc.h"
#include "../src/enc.c"
#include <string.h>

START_TEST(test_gen_key_pass)
{
	void *key, *iv;
	const void* password = "Who are you?\n I'm the Doctor.\n Doctor who? \n Precisely.";
	ck_assert_int_eq(gen_key(password, sizeof(password), key, iv), 0);
}
END_TEST

Suite* enc_suite(void) {
  Suite* suite = suite_create("enc");
  TCase* gen_key_tcase = tcase_create("Gen Key");

  tcase_add_test(gen_key_tcase, test_gen_key_pass);
  suite_add_tcase(suite, gen_key_tcase);

  return suite;
}
