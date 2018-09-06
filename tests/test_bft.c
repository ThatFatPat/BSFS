#include "test_bft.h"

#include "bft.h"
#include <errno.h>
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


START_TEST(test_bft_find_free_entry)
{
  uint8_t bft[BFT_ENTRY_SIZE * BFT_MAX_ENTRIES] = {0};
  
  bft_offset_t first_off;
  int first_status = bft_find_free_table_entry(bft, &first_off);
  ck_assert_int_eq(first_status, 0);
  ck_assert_uint_eq(first_off, 0);

  bft[0] = 'a'; // simulate in-use

  bft_offset_t second_off;
  int second_status = bft_find_free_table_entry(bft, &second_off);
  ck_assert_int_eq(second_status, 0);
  ck_assert_uint_eq(second_off, 1);
}
END_TEST

START_TEST(test_bft_find_free_entry_nospace)
{
  uint8_t bft[BFT_ENTRY_SIZE * BFT_MAX_ENTRIES];
  memset(bft, 'a', sizeof(bft)); // 'fill' BFT

  bft_offset_t off;
  int status = bft_find_free_table_entry(bft, &off);
  ck_assert_int_eq(status, -ENOSPC);
}
END_TEST


Suite* bft_suite(void) {
  Suite* suite = suite_create("bft");
  
  TCase* lifetime_tc = tcase_create("lifetime");
  tcase_add_test(lifetime_tc, test_bft_init_basic);
  tcase_add_test(lifetime_tc, test_bft_init_deep_copy);
  suite_add_tcase(suite, lifetime_tc);

  TCase* find_free_tc = tcase_create("bft_find_free_entry");
  tcase_add_test(find_free_tc, test_bft_find_free_entry);
  tcase_add_test(find_free_tc, test_bft_find_free_entry_nospace);
  suite_add_tcase(suite, find_free_tc);

  return suite;
}
