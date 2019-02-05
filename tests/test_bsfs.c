#include "test_bsfs.h"

#include "bsfs_priv.h"
#include <errno.h>

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

START_TEST(test_ftab_insert_multi_same_bucket) {
  bs_file_table_t table;
  ck_assert_int_eq(bs_file_table_init(&table), 0);

  bs_file_t file1;
  ck_assert_int_eq(bs_file_table_open(&table, NULL, 0, &file1), 0);
  bs_file_t file2;
  ck_assert_int_eq(bs_file_table_open(&table, NULL, table.bucket_count, &file2),
                   0);
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

START_TEST(test_ftab_lookup_multi_same_bucket) {
  bs_file_table_t table;
  ck_assert_int_eq(bs_file_table_init(&table), 0);

  bs_file_t file1;
  ck_assert_int_eq(bs_file_table_open(&table, NULL, 0, &file1), 0);
  bs_file_t file2;
  ck_assert_int_eq(bs_file_table_open(&table, NULL, table.bucket_count, &file2),
                   0);

  bs_file_t found1;
  ck_assert_int_eq(bs_file_table_open(&table, NULL, 0, &found1), 0);
  ck_assert_int_eq(found1->refcount, 2);

  bs_file_t found2;
  ck_assert_int_eq(
      bs_file_table_open(&table, NULL, table.bucket_count, &found2), 0);
  ck_assert_int_eq(found2->refcount, 2);

  bs_file_table_destroy(&table);
}
END_TEST

START_TEST(test_ftab_rehash) {
  bs_file_table_t table;
  ck_assert_int_eq(bs_file_table_init(&table), 0);

  size_t initial_bucket_count = table.bucket_count;
  bft_offset_t i = 0;

  // wait for rehash
  for (; table.bucket_count == initial_bucket_count; i++) {
    bs_file_t file;
    ck_assert_int_eq(bs_file_table_open(&table, NULL, i, &file), 0);
    ck_assert_int_eq(
        bs_file_table_open(&table, NULL, i + 2 * initial_bucket_count, &file),
        0);
  }

  // verify contents
  for (i--; i >= 0; i--) {
    bs_file_t file;
    ck_assert_int_eq(bs_file_table_open(&table, NULL, i, &file), 0);
    ck_assert_int_eq(file->refcount, 2);

    ck_assert_int_eq(
        bs_file_table_open(&table, NULL, i + 2 * initial_bucket_count, &file),
        0);
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

START_TEST(test_ftab_remove_empty) {
  bs_file_table_t table;
  ck_assert_int_eq(bs_file_table_init(&table), 0);

  bs_file_t file;
  ck_assert_int_eq(bs_file_table_open(&table, NULL, 123, &file), 0);
  ck_assert_int_eq(bs_file_table_release(&table, file), 0);

  ck_assert_ptr_eq(table.head, NULL);
  for (size_t i = 0; i < table.bucket_count; i++) {
    ck_assert_ptr_eq(table.buckets[i], NULL);
  }

  bs_file_table_destroy(&table);
}
END_TEST

START_TEST(test_ftab_remove_invalid) {
  bs_file_table_t table;
  ck_assert_int_eq(bs_file_table_init(&table), 0);

  struct bs_file_impl raw_file = { .refcount = 1 };
  ck_assert_int_eq(bs_file_table_release(&table, &raw_file), -EINVAL);

  bs_file_table_destroy(&table);
}
END_TEST

START_TEST(test_ftab_remove_size) {
  bs_file_table_t table;
  ck_assert_int_eq(bs_file_table_init(&table), 0);

  bs_file_t file;
  ck_assert_int_eq(bs_file_table_open(&table, NULL, 123, &file), 0);
  ck_assert_int_eq(bs_file_table_release(&table, file), 0);
  ck_assert_int_eq(table.size, 0);

  bs_file_table_destroy(&table);
}
END_TEST

START_TEST(test_ftab_release_decref) {
  bs_file_table_t table;
  ck_assert_int_eq(bs_file_table_init(&table), 0);

  bs_file_t file;
  ck_assert_int_eq(bs_file_table_open(&table, NULL, 123, &file), 0);
  ck_assert_int_eq(bs_file_table_open(&table, NULL, 123, &file), 0);
  ck_assert_int_eq(bs_file_table_release(&table, file), 0);
  ck_assert_int_eq(file->refcount, 1);

  bs_file_table_destroy(&table);
}
END_TEST

START_TEST(test_ftab_remove_same_bucket) {
  bs_file_table_t table;
  ck_assert_int_eq(bs_file_table_init(&table), 0);

  bs_file_t file1;
  ck_assert_int_eq(bs_file_table_open(&table, NULL, 0, &file1), 0);

  bs_file_t file2;
  ck_assert_int_eq(bs_file_table_open(&table, NULL, table.bucket_count, &file2),
                   0);

  ck_assert_int_eq(bs_file_table_release(&table, file1), 0);

  bs_file_t file3;
  ck_assert_int_eq(bs_file_table_open(&table, NULL, table.bucket_count, &file3),
                   0);
  ck_assert_int_eq(file3->refcount, 2);

  bs_file_table_destroy(&table);
}
END_TEST

START_TEST(test_ftab_remove_bucket_head) {
  bs_file_table_t table;
  ck_assert_int_eq(bs_file_table_init(&table), 0);

  bs_file_t file1;
  ck_assert_int_eq(bs_file_table_open(&table, NULL, 0, &file1), 0);

  bs_file_t file2;
  ck_assert_int_eq(bs_file_table_open(&table, NULL, table.bucket_count, &file2),
                   0);

  ck_assert_int_eq(bs_file_table_release(&table, file2), 0);

  bs_file_t file3;
  ck_assert_int_eq(bs_file_table_open(&table, NULL, 0, &file3), 0);
  ck_assert_int_eq(file3->refcount, 2);

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
  tcase_add_test(ftab_tcase, test_ftab_insert_multi_same_bucket);
  tcase_add_test(ftab_tcase, test_ftab_insert_multi_size);
  tcase_add_test(ftab_tcase, test_ftab_lookup_multi_same_bucket);
  tcase_add_test(ftab_tcase, test_ftab_rehash);
  tcase_add_test(ftab_tcase, test_ftab_insert_after_rehash);
  tcase_add_test(ftab_tcase, test_ftab_remove_empty);
  tcase_add_test(ftab_tcase, test_ftab_remove_invalid);
  tcase_add_test(ftab_tcase, test_ftab_remove_size);
  tcase_add_test(ftab_tcase, test_ftab_release_decref);
  tcase_add_test(ftab_tcase, test_ftab_remove_same_bucket);
  tcase_add_test(ftab_tcase, test_ftab_remove_bucket_head);
  suite_add_tcase(suite, ftab_tcase);

  return suite;
}
