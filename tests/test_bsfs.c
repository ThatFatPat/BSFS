#include "test_bsfs.h"

#include "bsfs_priv.h"

START_TEST(test_ftab_insert_lookup) {
  bs_file_table_t table;
  ck_assert_int_eq(bs_file_table_init(&table), 0);

  bs_file_t created;
  ck_assert_int_eq(bs_file_table_open(&table, NULL, 123, &created), 0);
  bs_file_t found;
  ck_assert_int_eq(bs_file_table_open(&table, NULL, 123, &found), 0);
  ck_assert_ptr_eq(created, found);

  bs_file_table_destroy(&table);
}
END_TEST

START_TEST(test_ftab_insert_size) {
  bs_file_table_t table;
  ck_assert_int_eq(bs_file_table_init(&table), 0);

  bs_file_t file;
  ck_assert_int_eq(bs_file_table_open(&table, NULL, 123, &file), 0);
  ck_assert_uint_eq(table.size, 1);

  bs_file_table_destroy(&table);
}
END_TEST

START_TEST(test_ftab_insert_lookup_refcount) {
  bs_file_table_t table;
  ck_assert_int_eq(bs_file_table_init(&table), 0);

  bs_file_t file1;
  ck_assert_int_eq(bs_file_table_open(&table, NULL, 123, &file1), 0);
  ck_assert_int_eq(file1->refcount, 1);

  bs_file_t file2;
  ck_assert_int_eq(bs_file_table_open(&table, NULL, 123, &file2), 0);
  ck_assert_ptr_eq(file1, file2);
  ck_assert_int_eq(file2->refcount, 2);

  bs_file_table_destroy(&table);
}
END_TEST

Suite* bsfs_suite(void) {
  Suite* suite = suite_create("bsfs");

  TCase* ftab_tcase = tcase_create("ftab");
  tcase_add_test(ftab_tcase, test_ftab_insert_lookup);
  tcase_add_test(ftab_tcase, test_ftab_insert_size);
  tcase_add_test(ftab_tcase, test_ftab_insert_lookup_refcount);
  suite_add_tcase(suite, ftab_tcase);

  return suite;
}
