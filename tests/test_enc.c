#include "test_enc.h"
#include "enc.h"
#include <string.h>


Suite* enc_suite(void) {
  Suite* suite = suite_create("enc");
  //TCase* gen_key_tcase = tcase_create("Gen Key");

  //tcase_add_test(gen_key_tcase, test_gen_key_pass);
  //suite_add_tcase(suite, gen_key_tcase);

  return suite;
}
