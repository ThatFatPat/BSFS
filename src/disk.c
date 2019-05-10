#include "disk.h"

#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

struct bs_disk_impl {
  int fd;
  void* data;
  size_t data_size;
  pthread_rwlock_t lock; // synchronize access to `data` across threads
};

int disk_create(int fd, bs_disk_t* disk) {
  int ret;
  int ret_pthread;
  *disk = NULL;

  bs_disk_t disk_local = (bs_disk_t) malloc(sizeof(struct bs_disk_impl));
  if (!disk_local) {
    return -ENOMEM;
  }

  if (flock(fd, LOCK_EX | LOCK_NB) ==
      -1) { // Locks the file with file descriptor `fd`
            // to be EXCLUSIVE to THIS FD.
    ret = -errno;
    goto fail_after_alloc;
  }

  struct stat sb;
  if (fstat(fd, &sb) == -1) {
    ret = -errno;
    goto fail_after_flock;
  }

  disk_local->data =
      mmap(NULL, sb.st_size, PROT_READ | PROT_WRITE, MAP_ANON, fd, 0);
  if (disk_local->data == MAP_FAILED) {
    ret = -errno;
    goto fail_after_flock;
  }

  ret_pthread = pthread_rwlock_init(&disk_local->lock, NULL);
  if (ret_pthread) {
    ret = -ret_pthread;
    goto fail_after_mmap;
  }

  disk_local->data_size = sb.st_size;
  disk_local->fd = fd;
  *disk = disk_local;
  return 0;

// Failure
fail_after_mmap:
  munmap(disk_local->data, sb.st_size);
fail_after_flock:
  flock(fd, LOCK_UN);
fail_after_alloc:
  free(disk_local);
  return ret;
}

void disk_free(bs_disk_t disk) {
  pthread_rwlock_destroy(&disk->lock);
  munmap(disk->data, disk->data_size);
  flock(disk->fd, LOCK_UN);
  close(disk->fd);
  free(disk);
}

size_t disk_get_size(bs_disk_t disk) {
  return disk->data_size;
}

int disk_lock_read(bs_disk_t disk, const void** data) {
  pthread_rwlock_rdlock(&disk->lock);
  *data = disk->data;
  return 0;
}

int disk_unlock_read(bs_disk_t disk) {
  return -pthread_rwlock_unlock(&disk->lock);
}

int disk_lock_write(bs_disk_t disk, void** data) {
  pthread_rwlock_wrlock(&disk->lock);
  *data = disk->data;
  return 0;
}

int disk_unlock_write(bs_disk_t disk) {
  return disk_unlock_read(disk);
}
