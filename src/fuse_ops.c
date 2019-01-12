#include "fuse_ops.h"

#include "keytab.h"
#include "disk.h"
#include <pthread.h>

struct bs_file_impl{
    struct bs_open_level_impl* level;
    bft_index index;
    _Atomic int refcount; // Atomically counts the amount of references to file.
    pthread_rwlock_t file_lock;

};

struct bs_open_level_impl{
    struct bs_bsfs_impl bsfs;
    char* key;
    char* pass;
    void* bft;
    void* bitmap;
    // TODO: open_files
    pthread_mutex_t ftab_lock; // Locks the Open File Table.
    pthread_rwlock_t metadata_lock; // Used for locking BFT and Bitmap.
};

struct bs_bsfs_impl{
    struct bs_open_level_impl levels[MAX_LEVELS];
    struct bs_disk_t;
    pthread_mutex_t level_lock;
};