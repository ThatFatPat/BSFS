#ifndef BS_DISK_API_H
#define BS_DISK_API_H

typedef struct _disk bs_disk_t;

int disk_create(int fd, bs_disk_t *disk);
void disk_free(bs_disk_t disk);
size_t disk_get_size(bs_disk_t disk);
int disk_lock_read(bs_disk_t disk, const void **data);
int disk_unlock_read(bs_disk_t disk);
int disk_lock_write(bs_disk_t disk, void **data);
int disk_unlock_write(bs_disk_t disk);

#endif //BS_DISK_API_H
