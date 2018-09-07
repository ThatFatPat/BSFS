#include "disk_api.h"
#include <stdbool.h>
#include <stddef.h>
#include <errno.h>

struct _disk {
  int fd;
  void* data;
  size_t data_size;
  bool read_lock;
  bool write_lock;
};

int disk_create(int fd, bs_disk_t *disk){
  return -ENOSYS;
}

void disk_free(bs_disk_t disk){
}

size_t disk_get_size(bs_disk_t disk){
  return -1;
}

int disk_lock_read(bs_disk_t disk, const void **data){
  return -ENOSYS;
}

int disk_unlock_read(bs_disk_t disk){
  return -ENOSYS;
}

int disk_lock_write(bs_disk_t disk, void **data){
  return -ENOSYS;
}

int disk_unlock_write(bs_disk_t disk){
  return -ENOSYS;
}
