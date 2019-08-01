#include "cluster.h"

#include "bft.h"
#include "bit_util.h"
#include <errno.h>
#include <stddef.h>
#include <stdint.h>

#define BITS_PER_BLOCK (16 * CHAR_BIT)

size_t fs_count_clusters(size_t level_size) {
  if (level_size <= BFT_SIZE) {
    return 0;
  }

  size_t bits_after_bft = CHAR_BIT * (level_size - BFT_SIZE);
  if (bits_after_bft < BITS_PER_BLOCK - 1) {
    return 0;
  }

  return (bits_after_bft - BITS_PER_BLOCK + 1) / (CHAR_BIT * CLUSTER_SIZE + 1);
}

size_t fs_compute_bitmap_size(size_t clusters) {
  return 16 * ((round_to_bytes(clusters) + 15) / 16);
}

size_t fs_compute_bitmap_size_from_disk(bs_disk_t disk) {
  return fs_compute_bitmap_size(
      fs_count_clusters(stego_compute_user_level_size(disk_get_size(disk))));
}

int fs_read_cluster(const stego_key_t* key, bs_disk_t disk, void* buf,
                    cluster_offset_t cluster) {
  return stego_read_level(key, disk, buf,
                          BFT_SIZE + fs_compute_bitmap_size_from_disk(disk) +
                              CLUSTER_SIZE * cluster,
                          CLUSTER_SIZE);
}

int fs_write_cluster(const stego_key_t* key, bs_disk_t disk, const void* buf,
                     cluster_offset_t cluster) {
  return stego_write_level(key, disk, buf,
                           BFT_SIZE + fs_compute_bitmap_size_from_disk(disk) +
                               CLUSTER_SIZE * cluster,
                           CLUSTER_SIZE);
}

cluster_offset_t fs_next_cluster(const void* cluster) {
  return read_be32((const uint8_t*) cluster + CLUSTER_DATA_SIZE);
}

void fs_set_next_cluster(void* cluster, cluster_offset_t next) {
  write_be32((uint8_t*) cluster + CLUSTER_DATA_SIZE, next);
}

int fs_read_bitmap(const stego_key_t* key, bs_disk_t disk, void* buf) {
  return stego_read_level(key, disk, buf, BFT_SIZE,
                          fs_compute_bitmap_size_from_disk(disk));
}

int fs_write_bitmap(const stego_key_t* key, bs_disk_t disk, const void* buf) {
  return stego_write_level(key, disk, buf, BFT_SIZE,
                           fs_compute_bitmap_size_from_disk(disk));
}

int fs_alloc_cluster(void* bitmap, size_t bitmap_bits, cluster_offset_t start,
                     cluster_offset_t* new_cluster) {
  for (cluster_offset_t cluster = start; cluster < bitmap_bits; cluster++) {
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
