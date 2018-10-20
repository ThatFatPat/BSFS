#include "cluster.h"

#include "bft.h"
#include "bit_util.h"
#include "stego.h"
#include <errno.h>
#include <stddef.h>
#include <stdint.h>

static size_t compute_bitmap_size_from_disk(bs_disk_t disk) {
  return fs_compute_bitmap_size(compute_level_size(disk_get_size(disk)));
}

size_t fs_count_clusters(size_t level_size) {
  if (level_size <= BFT_SIZE) {
    return 0;
  }
  return (128 * (level_size - BFT_SIZE) - 127) / (128 * CLUSTER_SIZE + 1);
}

size_t fs_compute_bitmap_size(size_t level_size) {
  return ((fs_count_clusters(level_size) + CHAR_BIT - 1) / CHAR_BIT + 15) / 16;
}

int fs_read_cluster(const void* key, bs_disk_t disk, void* buf,
                    cluster_offset_t cluster) {
  return stego_read_level(key, disk, buf,
                          BFT_SIZE + compute_bitmap_size_from_disk(disk) +
                              CLUSTER_SIZE * cluster,
                          CLUSTER_SIZE);
}

int fs_write_cluster(const void* key, bs_disk_t disk, const void* buf,
                     cluster_offset_t cluster) {
  return stego_write_level(key, disk, buf,
                           BFT_SIZE + compute_bitmap_size_from_disk(disk) +
                               CLUSTER_SIZE * cluster,
                           CLUSTER_SIZE);
}

cluster_offset_t fs_next_cluster(const void* cluster) {
  return read_big_endian((const uint8_t*) cluster + CLUSTER_DATA_SIZE);
}

void fs_set_next_cluster(void* cluster, cluster_offset_t next) {
  write_big_endian((uint8_t*) cluster + CLUSTER_DATA_SIZE, next);
}

int fs_read_bitmap(const void* key, bs_disk_t disk, void* buf) {
  return stego_read_level(key, disk, buf, BFT_SIZE,
                          compute_bitmap_size_from_disk(disk));
}

int fs_write_bitmap(const void* key, bs_disk_t disk, const void* buf) {
  return stego_write_level(key, disk, buf, BFT_SIZE,
                           compute_bitmap_size_from_disk(disk));
}

int fs_alloc_cluster(void* bitmap, size_t bitmap_bits,
                     cluster_offset_t* new_cluster) {
  for (cluster_offset_t cluster = 0; cluster < bitmap_bits; cluster++) {
    if (!get_bit(bitmap, cluster)) {
      set_bit(bitmap, cluster, 1);
      *new_cluster = cluster;
      return 0;
    }
  }

  return -ENOSPC;
}

int fs_dealloc_cluster(void* bitmap, size_t bitmap_bits,
                       cluster_offset_t cluster) {
  if (cluster >= bitmap_bits) {
    return -EINVAL;
  }
  set_bit(bitmap, cluster, 0);
  return 0;
}
