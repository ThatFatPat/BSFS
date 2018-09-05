#include "test_bft.h"

#include "bft.h"
#include <string.h>

START_TEST(test_bft_init_basic)
{
  bft_entry_t ent;
  bft_entry_init(&ent, "Test", 42, 0, 1, 2, 3);
  
  ck_assert_str_eq(ent.name, "Test");
  ck_assert_uint_eq(ent.size, 42);
  ck_assert_uint_eq(ent.mode, 0);
  ck_assert_uint_eq(ent.initial_cluster, 1);
  ck_assert_uint_eq(ent.atim, 2);
  ck_assert_uint_eq(ent.mtim, 3);

  bft_entry_destroy(&ent);
}
END_TEST

START_TEST(test_bft_init_deep_copy)
{
  const char* name = "entry";
  bft_entry_t ent;
  bft_entry_init(&ent, name, 0, 0, 0, 0, 0);
  ck_assert_ptr_ne(ent.name, name);
  bft_entry_destroy(&ent);
}
END_TEST


Suite* bft_suite(void) {
  Suite* suite = suite_create("bft");
  
  TCase* lifetime_tc = tcase_create("lifetime");
  tcase_add_test(lifetime_tc, test_bft_init_basic);
  tcase_add_test(lifetime_tc, test_bft_init_deep_copy);
  suite_add_tcase(suite, lifetime_tc);

  return suite;
}
