#ifndef BS_CLUSTER_H
#define BS_CLUSTER_H

#include "disk.h"
#include <stdint.h>
#include <stddef.h>

typedef uint32_t cluster_offset_t;
#define CLUSTER_OFFSET_EOF UINT32_MAX
#define CLUSTER_SIZE 2048
#define CLUSTER_DATA_SIZE (CLUSTER_SIZE - sizeof(cluster_offset_t))

size_t fs_count_clusters(size_t level_size);

int fs_read_cluster(const void* key, bs_disk_t disk, void* buf, cluster_offset_t cluster);
int fs_write_cluster(const void* key, bs_disk_t disk, const void* buf, cluster_offset_t cluster);

cluster_offset_t fs_next_cluster(const void* cluster);

int fs_read_bitmap(const void* key, bs_disk_t disk, void* buf, size_t bitmap_size);
int fs_write_bitmap(const void* key, bs_disk_t disk, const void* buf, size_t bitmap_size);

int fs_alloc_cluster(void* bitmap, cluster_offset_t* new_cluster);
int fs_dealloc_cluster(void* bitmap, cluster_offset_t cluster);

#endif //BS_CLUSTER_H
