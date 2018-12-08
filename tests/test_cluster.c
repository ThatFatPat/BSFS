#include "test_cluster.h"

#include "bft.h"
#include "cluster.h"
#include "disk.h"
#include "stego.h"
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>

START_TEST(test_compute_bitmap_size) {
  for (size_t clusters = 0; clusters < 512; clusters++) {
    size_t bitmap_size = fs_compute_bitmap_size(clusters);
    ck_assert_uint_eq(bitmap_size % 16, 0);
    ck_assert_uint_le(clusters, bitmap_size * CHAR_BIT);
    ck_assert_uint_lt(bitmap_size * CHAR_BIT - clusters, 128);
  }
}
END_TEST

START_TEST(test_count_clusters) {
  for (size_t level_size = BFT_SIZE; level_size < 0x200000;
       level_size += 0x8000) {
    size_t clusters = fs_count_clusters(level_size);
    size_t bitmap_size = fs_compute_bitmap_size(clusters);

    ck_assert_uint_le(BFT_SIZE + bitmap_size + clusters * CLUSTER_SIZE,
                      level_size);
  }
}
END_TEST

START_TEST(test_get_set_next_cluster_roundtrip) {
  static uint8_t expected_data[CLUSTER_DATA_SIZE] = { 0 };

  uint8_t cluster[CLUSTER_SIZE] = { 0 };
  fs_set_next_cluster(cluster, 0xdeadbeef);
  ck_assert_int_eq(memcmp(cluster, expected_data, CLUSTER_DATA_SIZE), 0);
  ck_assert_uint_eq(fs_next_cluster(cluster), 0xdeadbeef);
}
END_TEST

#define CLUSTERS 16

START_TEST(test_bitmap_alloc_cluster) {
  uint8_t bitmap[fs_compute_bitmap_size(CLUSTERS)];
  memset(bitmap, 0, sizeof(bitmap));

  cluster_offset_t first;
  ck_assert_int_eq(fs_alloc_cluster(bitmap, CLUSTERS, &first), 0);

  cluster_offset_t second;
  ck_assert_int_eq(fs_alloc_cluster(bitmap, CLUSTERS, &second), 0);

  ck_assert_uint_ne(first, second);
}
END_TEST

START_TEST(test_bitmap_alloc_cluster_nospace) {
  uint8_t bitmap[fs_compute_bitmap_size(CLUSTERS)];
  memset(bitmap, -1, sizeof(bitmap)); // all bits 1 => full

  cluster_offset_t clus;
  ck_assert_int_eq(fs_alloc_cluster(bitmap, CLUSTERS, &clus), -ENOSPC);
}
END_TEST

START_TEST(test_bitmap_dealloc_cluster) {
  uint8_t bitmap[fs_compute_bitmap_size(CLUSTERS)];
  memset(bitmap, -1, sizeof(bitmap)); // all bits 1 => full

  cluster_offset_t clus;
  ck_assert_int_eq(fs_alloc_cluster(bitmap, CLUSTERS, &clus), -ENOSPC);

  ck_assert_int_eq(fs_dealloc_cluster(bitmap, CLUSTERS, 5), 0);
  ck_assert_int_eq(fs_alloc_cluster(bitmap, CLUSTERS, &clus), 0);
  ck_assert_uint_eq(clus, 5);
}
END_TEST

START_TEST(test_bitmap_dealloc_cluster_past_end) {
  uint8_t bitmap[fs_compute_bitmap_size(CLUSTERS)];
  memset(bitmap, 0, sizeof(bitmap));

  ck_assert_int_eq(fs_dealloc_cluster(bitmap, CLUSTERS, CLUSTERS + 1), -EINVAL);
}
END_TEST

Suite* cluster_suite(void) {
  Suite* suite = suite_create("cluster");

  TCase* count_tcase = tcase_create("count");
  tcase_add_test(count_tcase, test_compute_bitmap_size);
  tcase_add_test(count_tcase, test_count_clusters);
  suite_add_tcase(suite, count_tcase);

  TCase* next_tcase = tcase_create("next");
  tcase_add_test(next_tcase, test_get_set_next_cluster_roundtrip);
  suite_add_tcase(suite, next_tcase);

  TCase* bitmap_tcase = tcase_create("bitmap");
  tcase_add_test(bitmap_tcase, test_bitmap_alloc_cluster);
  tcase_add_test(bitmap_tcase, test_bitmap_alloc_cluster_nospace);
  tcase_add_test(bitmap_tcase, test_bitmap_dealloc_cluster);
  tcase_add_test(bitmap_tcase, test_bitmap_dealloc_cluster_past_end);
  suite_add_tcase(suite, bitmap_tcase);

  return suite;
}
