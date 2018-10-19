#include "cluster.h"

#include "bft.h"
#include "bit_util.h"
#include "stego.h"
#include <stddef.h>
#include <stdint.h>

size_t fs_count_clusters(size_t level_size) {
}

size_t fs_compute_bitmap_size(size_t level_size) {
  return (fs_count_clusters(level_size) / CHAR_BIT + 15) / 16;
}

int fs_read_cluster(const void* key, bs_disk_t disk, void* buf,
                    cluster_offset_t cluster) {
}

int fs_write_cluster(const void* key, bs_disk_t disk, const void* buf,
                     cluster_offset_t cluster) {
}

cluster_offset_t fs_next_cluster(const void* cluster) {
  return read_big_endian((const uint8_t*) cluster + CLUSTER_DATA_SIZE);
}

void fs_set_next_cluster(void* cluster, cluster_offset_t next) {
  write_big_endian((const uint8_t*) cluster + CLUSTER_DATA_SIZE, next);
}

int fs_read_bitmap(const void* key, bs_disk_t disk, void* buf,
                   size_t bitmap_size) {
  return stego_read_level(key, disk, buf, BFT_SIZE, bitmap_size);
}

int fs_write_bitmap(const void* key, bs_disk_t disk, const void* buf,
                    size_t bitmap_size) {
}

int fs_alloc_cluster(void* bitmap, cluster_offset_t* new_cluster) {
}

int fs_dealloc_cluster(void* bitmap, cluster_offset_t cluster) {
}
