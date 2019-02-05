#include "test_bsfs.h"

#include "bsfs_priv.h"

START_TEST(test_ftab_insert_vals) {
  bs_file_table_t table;
  ck_assert_int_eq(bs_file_table_init(&table), 0);

  struct bs_open_level_impl* const level =
      (struct bs_open_level_impl*) 0xdeadbeef;
  const bft_offset_t index = 123;

  bs_file_t file;
  ck_assert_int_eq(bs_file_table_open(&table, level, index, &file), 0);
  ck_assert_ptr_eq(file->level, level);
  ck_assert_int_eq(file->index, index);

  bs_file_table_destroy(&table);
}
END_TEST

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

START_TEST(test_ftab_insert_lookup_vals) {
  bs_file_table_t table;
  ck_assert_int_eq(bs_file_table_init(&table), 0);

  struct bs_open_level_impl* const level =
      (struct bs_open_level_impl*) 0xdeadbeef;
  const bft_offset_t index = 123;

  bs_file_t created;
  ck_assert_int_eq(bs_file_table_open(&table, level, index, &created), 0);
  bs_file_t found;
  ck_assert_int_eq(bs_file_table_open(&table, NULL, index, &found), 0);

  ck_assert_ptr_eq(found->level, level);

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

START_TEST(test_ftab_insert_multi) {
  bs_file_table_t table;
  ck_assert_int_eq(bs_file_table_init(&table), 0);

  bs_file_t file1;
  ck_assert_int_eq(bs_file_table_open(&table, NULL, 0, &file1), 0);
  bs_file_t file2;
  ck_assert_int_eq(bs_file_table_open(&table, NULL, 1, &file2), 0);
  ck_assert_ptr_ne(file1, file2);

  bs_file_table_destroy(&table);
}
END_TEST

START_TEST(test_ftab_insert_multi_size) {
  bs_file_table_t table;
  ck_assert_int_eq(bs_file_table_init(&table), 0);

  bs_file_t file1;
  ck_assert_int_eq(bs_file_table_open(&table, NULL, 0, &file1), 0);
  bs_file_t file2;
  ck_assert_int_eq(bs_file_table_open(&table, NULL, 1, &file2), 0);
  ck_assert_uint_eq(table.size, 2);

  bs_file_table_destroy(&table);
}
END_TEST

START_TEST(test_ftab_insert_multi_rehash) {
  bs_file_table_t table;
  ck_assert_int_eq(bs_file_table_init(&table), 0);

  size_t initial_bucket_count = table.bucket_count;
  bft_offset_t i = 0;

  // wait for rehash
  for (; table.bucket_count == initial_bucket_count; i++) {
    bs_file_t file;
    ck_assert_int_eq(bs_file_table_open(&table, NULL, i, &file), 0);
  }

  // verify contents
  for (i--; i >= 0; i--) {
    bs_file_t file;
    ck_assert_int_eq(bs_file_table_open(&table, NULL, i, &file), 0);
    ck_assert_int_eq(file->refcount, 2);
  }

  bs_file_table_destroy(&table);
}
END_TEST

START_TEST(test_ftab_insert_after_rehash) {
  bs_file_table_t table;
  ck_assert_int_eq(bs_file_table_init(&table), 0);

  size_t initial_bucket_count = table.bucket_count;
  bft_offset_t i = 0;

  // wait for rehash
  for (; table.bucket_count == initial_bucket_count; i++) {
    bs_file_t file;
    ck_assert_int_eq(bs_file_table_open(&table, NULL, i, &file), 0);
  }

  // insert new file
  bs_file_t file;
  ck_assert_int_eq(bs_file_table_open(&table, NULL, i, &file), 0);
  ck_assert_int_eq(file->refcount, 1);

  bs_file_table_destroy(&table);
}
END_TEST

Suite* bsfs_suite(void) {
  Suite* suite = suite_create("bsfs");

  TCase* ftab_tcase = tcase_create("ftab");
  tcase_add_test(ftab_tcase, test_ftab_insert_vals);
  tcase_add_test(ftab_tcase, test_ftab_insert_lookup);
  tcase_add_test(ftab_tcase, test_ftab_insert_lookup_vals);
  tcase_add_test(ftab_tcase, test_ftab_insert_size);
  tcase_add_test(ftab_tcase, test_ftab_insert_lookup_refcount);
  tcase_add_test(ftab_tcase, test_ftab_insert_multi);
  tcase_add_test(ftab_tcase, test_ftab_insert_multi_size);
  tcase_add_test(ftab_tcase, test_ftab_insert_multi_rehash);
  tcase_add_test(ftab_tcase, test_ftab_insert_after_rehash);
  suite_add_tcase(suite, ftab_tcase);

  return suite;
}
