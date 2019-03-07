#ifndef BS_CLUSTER_H
#define BS_CLUSTER_H

#include "disk.h"
#include "stego.h"
#include <stddef.h>
#include <stdint.h>

typedef uint32_t cluster_offset_t;

#define CLUSTER_OFFSET_EOF UINT32_MAX
#define CLUSTER_SIZE 2048
#define CLUSTER_DATA_SIZE (CLUSTER_SIZE - sizeof(cluster_offset_t))

size_t fs_count_clusters(size_t level_size);
size_t fs_compute_bitmap_size(size_t clusters);
size_t fs_compute_bitmap_size_from_disk(bs_disk_t disk);

int fs_read_cluster(const stego_key_t* key, bs_disk_t disk, void* buf,
                    cluster_offset_t cluster);
int fs_write_cluster(const stego_key_t* key, bs_disk_t disk, const void* buf,
                     cluster_offset_t cluster);

cluster_offset_t fs_next_cluster(const void* cluster);
void fs_set_next_cluster(void* cluster, cluster_offset_t next);

int fs_read_bitmap(const stego_key_t* key, bs_disk_t disk, void* buf);
int fs_write_bitmap(const stego_key_t* key, bs_disk_t disk, const void* buf);

int fs_alloc_cluster(void* bitmap, size_t bitmap_bits,
                     cluster_offset_t* new_cluster);
int fs_dealloc_cluster(void* bitmap, size_t bitmap_bits,
                       cluster_offset_t cluster);

#endif // BS_CLUSTER_H
