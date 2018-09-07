#include "disk_api.h"

#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <sys/stat.h>

struct bs_disk_impl {
  void* data;
  size_t data_size;
  pthread_rwlock_t lock;
};

int disk_create(int fd, bs_disk_t *disk) {
  int ret;
  *disk = NULL;

  bs_disk_t disk_local = (bs_disk_t) malloc(sizeof(struct bs_disk_impl));
  if (!disk_local) {
    return -ENOMEM;                
  }

  flock(fd, LOCK_EX); // Locks the file with file descriptor `fd` to be EXCLUSIVE to THIS FD.

  struct stat sb;
  if(fstat(fd, &sb) == -1) {
    ret = -errno;
    goto fail;
  }

  disk_local->data = mmap(NULL, sb.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (disk_local->data == MAP_FAILED) {
    ret = -errno;                    
    goto fail;
  }

  disk_local->data_size = sb.st_size;
  if (int ret_pthread = pthread_rwlock_init(&disk_local->lock, NULL)) {
    ret = -ret_pthread;
    goto fail;
  }

  *disk = disk_local;
  return 0;

  fail:
  free(disk_local);
  flock(fd, LOCK_UN);
  return ret;
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
