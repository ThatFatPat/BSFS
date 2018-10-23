#include "test_cluster.h"

#include "bft.h"
#include "cluster.h"
#include "disk.h"
#include "stego.h"
#include <limits.h>
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

static bs_disk_t create_tmp_disk(void) {
  int fd = syscall(SYS_memfd_create, "test_cluster.bsf", 0);
  ck_assert_int_ne(ftruncate(fd, 0x8000000), -1); // 128MiB

  bs_disk_t disk;
  ck_assert_int_eq(disk_create(fd, &disk), 0);
  return disk;
}

START_TEST(test_read_write_cluster_roundtrip) {
  uint8_t cluster1[CLUSTER_SIZE];
  uint8_t cluster2[CLUSTER_SIZE];
  uint8_t read_cluster[CLUSTER_SIZE];

  uint8_t key[STEGO_KEY_SIZE];
  ck_assert_int_eq(stego_gen_keys(key, 1), 0);

  bs_disk_t disk = create_tmp_disk();

  ck_assert_int_eq(fs_write_cluster(key, disk, cluster1, 0), 0);
  ck_assert_int_eq(fs_write_cluster(key, disk, cluster2, 1), 0);

  ck_assert_int_eq(fs_read_cluster(key, disk, read_cluster, 0), 0);
  ck_assert_int_eq(memcmp(read_cluster, cluster1, CLUSTER_SIZE), 0);

  ck_assert_int_eq(fs_read_cluster(key, disk, read_cluster, 1), 0);
  ck_assert_int_eq(memcmp(read_cluster, cluster2, CLUSTER_SIZE), 0);

  disk_free(disk);
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

Suite* cluster_suite(void) {
  Suite* suite = suite_create("cluster");

  TCase* count_tcase = tcase_create("count");
  tcase_add_test(count_tcase, test_compute_bitmap_size);
  tcase_add_test(count_tcase, test_count_clusters);
  suite_add_tcase(suite, count_tcase);

  TCase* read_write_tcase = tcase_create("read_write");
  tcase_add_test(read_write_tcase, test_read_write_cluster_roundtrip);
  suite_add_tcase(suite, read_write_tcase);

  TCase* next_tcase = tcase_create("next");
  tcase_add_test(next_tcase, test_get_set_next_cluster_roundtrip);
  suite_add_tcase(suite, next_tcase);

  return suite;
}
