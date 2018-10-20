#include "test_cluster.h"

#include "bft.h"
#include "cluster.h"
#include <limits.h>

START_TEST(test_compute_bitmap_size) {
  for (size_t clusters = 0; clusters < 512; clusters++) {
    size_t bitmap_size = fs_compute_bitmap_size(clusters);
    ck_assert_uint_eq(bitmap_size % 16, 0);
    ck_assert_uint_le(clusters, bitmap_size * CHAR_BIT);
  }
}
END_TEST

START_TEST(test_count_clusters) {
  for (size_t level_size = BFT_SIZE; level_size < 0x200000;
       level_size += 0x8000) {
    size_t clusters = fs_count_clusters(level_size);
    ck_assert_uint_le(BFT_SIZE + ((clusters + 7) / 8 + 15) / 16 +
                          clusters * CLUSTER_SIZE,
                      level_size);
  }
}
END_TEST

Suite* cluster_suite(void) {
  Suite* suite = suite_create("cluster");

  TCase* count_tcase = tcase_create("count");
  tcase_add_test(count_tcase, test_compute_bitmap_size);
  tcase_add_test(count_tcase, test_count_clusters);
  suite_add_tcase(suite, count_tcase);

  return suite;
}
