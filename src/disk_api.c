#include "disk_api.h"
#include <stdbool.h>
#include <errno.h>
#include <sys/stat.h>
#include <stdint.h>
#include <sys/mman.h>

struct _disk {
  void* data;
  size_t data_size;
  bool read_lock;
  bool write_lock;
};

int disk_create(int fd, bs_disk_t *disk){
  struct stat sb;

  if(fstat(fd, &sb) == -1){
    return -errno;
  }

  disk->data = mmap(NULL, sb.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (disk->data == MAP_FAILED){
    return -errno;
  }

  disk->data_size = sb.st_size;
  disk->read_lock = false;
  disk->write_lock = false;
  
  return 0;
}

void disk_free(bs_disk_t disk){
}

size_t disk_get_size(bs_disk_t disk){
  return disk.data_size;
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
