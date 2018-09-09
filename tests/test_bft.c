#include "test_bft.h"

#include "bft.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>

START_TEST(test_bft_init_basic)
{
  bft_entry_t ent;
  ck_assert_int_eq(bft_entry_init(&ent, "Test", 42, 0, 1, 2, 3), 0);
  
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
  ck_assert_int_eq(bft_entry_init(&ent, name, 0, 0, 0, 0, 0), 0);
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


START_TEST(test_bft_entry_roundtrip)
{
  uint8_t bft[BFT_ENTRY_SIZE * BFT_MAX_ENTRIES];

  bft_entry_t original;
  ck_assert_int_eq(bft_entry_init(&original, "file", 1024, 0, 1, 0, 0), 0);

  int write_status = bft_write_table_entry(bft, &original, 0);
  ck_assert_int_eq(write_status, 0);

  bft_entry_t read;
  int read_status = bft_read_table_entry(bft, &read, 0);
  ck_assert_int_eq(read_status, 0);

  ck_assert_str_eq(original.name, read.name);
  ck_assert_uint_eq(original.size, read.size);
  ck_assert_uint_eq(original.mode, read.mode);
  ck_assert_uint_eq(original.initial_cluster, read.initial_cluster);
  ck_assert_uint_eq(original.atim, read.atim);
  ck_assert_uint_eq(original.mtim, read.mtim);

  bft_entry_destroy(&read);
  bft_entry_destroy(&original);
}
END_TEST

START_TEST(test_bft_read_entry_past_end)
{
  uint8_t bft[BFT_ENTRY_SIZE * BFT_MAX_ENTRIES];

  bft_entry_t ent;
  ck_assert_int_eq(bft_read_table_entry(bft, &ent, BFT_MAX_ENTRIES), -EINVAL);
  bft_entry_destroy(&ent);
}
END_TEST

START_TEST(test_bft_write_entry_past_end)
{
  uint8_t bft[BFT_ENTRY_SIZE * BFT_MAX_ENTRIES];

  bft_entry_t ent;
  ck_assert_int_eq(bft_entry_init(&ent, "file", 0, 0, 0, 0, 0), 0);
  ck_assert_int_eq(bft_write_table_entry(bft, &ent, BFT_MAX_ENTRIES), -EINVAL);
  bft_entry_destroy(&ent);
}
END_TEST

START_TEST(test_bft_read_entry_corrupt)
{
  uint8_t bft[BFT_ENTRY_SIZE * BFT_MAX_ENTRIES];
  memset(bft, 'a', sizeof(bft));

  bft_entry_t ent;
  ck_assert_int_eq(bft_read_table_entry(bft, &ent, 0), -EIO);
  bft_entry_destroy(&ent);
}
END_TEST

START_TEST(test_bft_read_entry_deep_copy)
{
  void* bft = calloc(BFT_MAX_ENTRIES, BFT_ENTRY_SIZE);

  bft_entry_t orig_ent;
  ck_assert_int_eq(bft_entry_init(&orig_ent, "file", 0, 0, 0, 0, 0), 0);
  ck_assert_int_eq(bft_write_table_entry(bft, &orig_ent, 0), 0);

  bft_entry_t read_ent;
  ck_assert_int_eq(bft_read_table_entry(bft, &read_ent, 0), 0);
  ck_assert_ptr_ne(read_ent.name, bft);
  
  free(bft);

  // note: after free
  ck_assert_str_eq(read_ent.name, orig_ent.name);

  bft_entry_destroy(&read_ent);
  bft_entry_destroy(&orig_ent);
}
END_TEST

START_TEST(test_bft_write_entry_name_too_long)
{
  uint8_t bft[BFT_ENTRY_SIZE * BFT_MAX_ENTRIES];

  bft_entry_t ent;
  ck_assert_int_eq(bft_entry_init(
    &ent,
    "1234567890123456789012345678901234567890123456789012345678901234",
    0, 0, 0, 0, 0
  ), 0);
  ck_assert_int_eq(bft_write_table_entry(bft, &ent, 0), -ENAMETOOLONG);
  bft_entry_destroy(&ent);
}
END_TEST


START_TEST(test_bft_remove_entry)
{
  uint8_t bft[BFT_ENTRY_SIZE * BFT_MAX_ENTRIES] = {0};

  bft_offset_t free_off;
  ck_assert_int_eq(bft_find_free_table_entry(bft, &free_off), 0);
  ck_assert_uint_eq(free_off, 0);

  bft_entry_t ent;
  ck_assert_int_eq(bft_entry_init(&ent, "file", 0, 0, 0, 0, 0), 0);
  ck_assert_int_eq(bft_write_table_entry(bft, &ent, 0), 0);
  bft_entry_destroy(&ent);

  ck_assert_int_eq(bft_find_free_table_entry(bft, &free_off), 0);
  ck_assert_uint_ne(free_off, 0);

  ck_assert_int_eq(bft_remove_table_entry(bft, 0), 0);

  ck_assert_int_eq(bft_find_free_table_entry(bft, &free_off), 0);
  ck_assert_uint_eq(free_off, 0);
}
END_TEST

START_TEST(test_bft_remove_entry_past_end)
{
  uint8_t bft[BFT_ENTRY_SIZE * BFT_MAX_ENTRIES];
  ck_assert_int_eq(bft_remove_table_entry(bft, BFT_MAX_ENTRIES), -EINVAL);
}
END_TEST


static bool test_bft_iter_entries_iter(bft_offset_t off, const bft_entry_t* ent,
  void* ctx) {
  ck_assert_int_lt(off, 2);

  bft_entry_t* orig_ent = (bft_entry_t*) ctx + off;

  ck_assert_str_eq(ent->name, orig_ent->name);
  ck_assert_uint_eq(ent->size, orig_ent->size);
  ck_assert_uint_eq(ent->mode, orig_ent->mode);
  ck_assert_uint_eq(ent->initial_cluster, orig_ent->initial_cluster);
  ck_assert_uint_eq(ent->atim, orig_ent->atim);
  ck_assert_uint_eq(ent->mtim, orig_ent->mtim);

  return true;
}

START_TEST(test_bft_iter_entries)
{
  uint8_t bft[BFT_ENTRY_SIZE * BFT_MAX_ENTRIES] = {0};

  bft_entry_t entries[2];
  ck_assert_int_eq(bft_entry_init(&entries[0], "file1", 12, 0, 0, 567, 560), 0);
  ck_assert_int_eq(bft_entry_init(&entries[1], "file2", 50, 0, 3, 700, 690), 0);

  ck_assert_int_eq(bft_write_table_entry(bft, &entries[0], 0), 0);
  ck_assert_int_eq(bft_write_table_entry(bft, &entries[1], 1), 0);

  ck_assert_int_eq(
    bft_iter_table_entries(bft, test_bft_iter_entries_iter, entries), 0
  );

  bft_entry_destroy(&entries[0]);
  bft_entry_destroy(&entries[1]);
}
END_TEST


struct test_bft_iter_bailout_ctx {
  int iter;
  bool bail;
};

static bool test_bft_iter_bailout_iter(bft_offset_t off, const bft_entry_t* ent,
  void* raw_ctx) {
  (void) off;
  (void) ent;

  struct test_bft_iter_bailout_ctx* ctx =
    (struct test_bft_iter_bailout_ctx*) raw_ctx;
  
  ctx->iter++;
  if (ctx->iter > 1 && ctx->bail) {
    return false;
  }
  return true;
}

START_TEST(test_bft_iter_bailout)
{
  uint8_t bft[BFT_ENTRY_SIZE * BFT_MAX_ENTRIES] = {0};
  bft_entry_t ent;

  ck_assert_int_eq(bft_entry_init(&ent, "file1", 10, 0, 0, 0, 0), 0);
  ck_assert_int_eq(bft_write_table_entry(bft, &ent, 0), 0);
  bft_entry_destroy(&ent);

  ck_assert_int_eq(bft_entry_init(&ent, "file2", 50, 0, 1, 0, 0), 0);
  ck_assert_int_eq(bft_write_table_entry(bft, &ent, 1), 0);
  bft_entry_destroy(&ent);

  ck_assert_int_eq(bft_entry_init(&ent, "file3", 500, 0, 2, 0, 0), 0);
  ck_assert_int_eq(bft_write_table_entry(bft, &ent, 2), 0);
  bft_entry_destroy(&ent);

  struct test_bft_iter_bailout_ctx ctx = {
    .iter = 0,
    .bail = false
  };
  ck_assert_int_eq(
    bft_iter_table_entries(bft, test_bft_iter_bailout_iter, &ctx), 0
  );
  ck_assert_int_eq(ctx.iter, 3);

  ctx = (struct test_bft_iter_bailout_ctx) {
    .iter = 0,
    .bail = true
  };
  ck_assert_int_eq(
    bft_iter_table_entries(bft, test_bft_iter_bailout_iter, &ctx), 0
  );
  ck_assert_int_eq(ctx.iter, 2);
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

  TCase* readwrite_tc = tcase_create("read_write_entry");
  tcase_add_test(readwrite_tc, test_bft_entry_roundtrip);
  tcase_add_test(readwrite_tc, test_bft_read_entry_past_end);
  tcase_add_test(readwrite_tc, test_bft_write_entry_past_end);
  tcase_add_test(readwrite_tc, test_bft_read_entry_corrupt);
  tcase_add_test(readwrite_tc, test_bft_read_entry_deep_copy);
  tcase_add_test(readwrite_tc, test_bft_write_entry_name_too_long);
  suite_add_tcase(suite, readwrite_tc);

  TCase* remove_tc = tcase_create("remove_entry");
  tcase_add_test(remove_tc, test_bft_remove_entry);
  tcase_add_test(remove_tc, test_bft_remove_entry_past_end);
  suite_add_tcase(suite, remove_tc);

  TCase* iter_tc = tcase_create("iter_entries");
  tcase_add_test(iter_tc, test_bft_iter_entries);
  tcase_add_test(iter_tc, test_bft_iter_bailout);
  suite_add_tcase(suite, iter_tc);

  return suite;
}
