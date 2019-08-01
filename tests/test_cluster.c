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

  stego_key_t key;
  ck_assert_int_eq(stego_gen_user_keys(&key, 1), 0);

  bs_disk_t disk = create_tmp_disk();

  ck_assert_int_eq(fs_write_cluster(&key, disk, cluster1, 0), 0);
  ck_assert_int_eq(fs_write_cluster(&key, disk, cluster2, 1), 0);

  ck_assert_int_eq(fs_read_cluster(&key, disk, read_cluster, 0), 0);
  ck_assert_int_eq(memcmp(read_cluster, cluster1, CLUSTER_SIZE), 0);

  ck_assert_int_eq(fs_read_cluster(&key, disk, read_cluster, 1), 0);
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

START_TEST(test_read_write_bitmap_roundtrip) {
  stego_key_t key;
  ck_assert_int_eq(stego_gen_user_keys(&key, 1), 0);

  bs_disk_t disk = create_tmp_disk();
  size_t bitmap_size = fs_compute_bitmap_size(
      fs_count_clusters(stego_compute_user_level_size(disk_get_size(disk))));

  void* bitmap = malloc(bitmap_size);
  void* read_bitmap = malloc(bitmap_size);

  memset(bitmap, 0, bitmap_size);
  strcpy((char*) bitmap, "abcdefghijklmnopqrstuvwxyz");

  ck_assert_int_eq(fs_write_bitmap(&key, disk, bitmap), 0);
  ck_assert_int_eq(fs_read_bitmap(&key, disk, read_bitmap), 0);

  ck_assert_int_eq(memcmp(read_bitmap, bitmap, bitmap_size), 0);

  free(read_bitmap);
  free(bitmap);
  disk_free(disk);
}
END_TEST

#define CLUSTERS 16

START_TEST(test_bitmap_alloc_cluster) {
  uint8_t bitmap[fs_compute_bitmap_size(CLUSTERS)];
  memset(bitmap, 0, sizeof(bitmap));

  cluster_offset_t first;
  ck_assert_int_eq(fs_alloc_cluster(bitmap, CLUSTERS, 0, &first), 0);

  cluster_offset_t second;
  ck_assert_int_eq(fs_alloc_cluster(bitmap, CLUSTERS, 0, &second), 0);

  ck_assert_uint_ne(first, second);
}
END_TEST

START_TEST(test_bitmap_alloc_cluster_nospace) {
  uint8_t bitmap[fs_compute_bitmap_size(CLUSTERS)];
  memset(bitmap, -1, sizeof(bitmap)); // all bits 1 => full

  cluster_offset_t clus;
  ck_assert_int_eq(fs_alloc_cluster(bitmap, CLUSTERS, 0, &clus), -ENOSPC);
}
END_TEST

START_TEST(test_bitmap_alloc_cluster_start) {
  uint8_t bitmap[fs_compute_bitmap_size(CLUSTERS)];
  memset(bitmap, 0, sizeof(bitmap));

  cluster_offset_t clus;
  ck_assert_int_eq(fs_alloc_cluster(bitmap, CLUSTERS, 12, &clus), 0);
  ck_assert_uint_eq(clus, 12);
}
END_TEST

START_TEST(test_bitmap_alloc_cluster_nospace_after_start) {
  uint8_t bitmap[fs_compute_bitmap_size(CLUSTERS)];
  memset(bitmap + 1, -1, sizeof(bitmap));

  cluster_offset_t clus;
  ck_assert_int_eq(fs_alloc_cluster(bitmap, CLUSTERS, 0, &clus), 0);
  ck_assert_int_eq(fs_alloc_cluster(bitmap, CLUSTERS, 15, &clus), -ENOSPC);
}
END_TEST

START_TEST(test_bitmap_dealloc_cluster) {
  uint8_t bitmap[fs_compute_bitmap_size(CLUSTERS)];
  memset(bitmap, -1, sizeof(bitmap)); // all bits 1 => full

  cluster_offset_t clus;
  ck_assert_int_eq(fs_alloc_cluster(bitmap, CLUSTERS, 0, &clus), -ENOSPC);

  ck_assert_int_eq(fs_dealloc_cluster(bitmap, CLUSTERS, 5), 0);
  ck_assert_int_eq(fs_alloc_cluster(bitmap, CLUSTERS, 0, &clus), 0);
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

  TCase* read_write_tcase = tcase_create("read_write");
  tcase_add_test(read_write_tcase, test_read_write_cluster_roundtrip);
  suite_add_tcase(suite, read_write_tcase);

  TCase* next_tcase = tcase_create("next");
  tcase_add_test(next_tcase, test_get_set_next_cluster_roundtrip);
  suite_add_tcase(suite, next_tcase);

  TCase* bitmap_tcase = tcase_create("bitmap");
  tcase_add_test(bitmap_tcase, test_read_write_bitmap_roundtrip);
  tcase_add_test(bitmap_tcase, test_bitmap_alloc_cluster);
  tcase_add_test(bitmap_tcase, test_bitmap_alloc_cluster_nospace);
  tcase_add_test(bitmap_tcase, test_bitmap_alloc_cluster_start);
  tcase_add_test(bitmap_tcase, test_bitmap_alloc_cluster_nospace_after_start);
  tcase_add_test(bitmap_tcase, test_bitmap_dealloc_cluster);
  tcase_add_test(bitmap_tcase, test_bitmap_dealloc_cluster_past_end);
  suite_add_tcase(suite, bitmap_tcase);

  return suite;
}
