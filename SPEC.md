# API:
> **All functions should return a (negative) error code on failure, 0 on success.**

## Disk:

### bs_disk_t:
```c
typedef /* unspecified */ bs_disk_t;
```
An opaque type representing a disk - usable only with disk APIs.

### disk_create:
```c
int disk_create(int fd, bs_disk_t* disk);
```
Create a disk backed by the file referenced by `fd`. Takes ownership of `fd`.

**Note**: Be sure to call `disk_free` when done, if the function succeeded.

### disk_free:
```c
void disk_free(bs_disk_t disk);
```
Clean up `disk`, releasing any resources held by it.

### disk_get_size:
```c
size_t disk_get_size(bs_disk_t disk);
```
Retrieve the size of `disk`.

### disk_lock_read:
```c
int disk_lock_read(bs_disk_t disk, const void** data);
```
Lock `disk` for reading, returning a buffer which can be used to read data on the disk.

**Note**: Multiple threads may lock a disk for reading simultaneously, but only a single thread can lock a disk for writing at a time.

**Note**: Every successful call to `disk_lock_read` **must** be paired with a call to `disk_unlock_read`. Failure to do so can result in deadlock.

### disk_unlock_read:
```c
int disk_unlock_read(bs_disk_t disk);
```
Unlock a disk previously locked for reading on this thread.

**Note**: After calling this function, the buffer returned by `disk_lock_read` should be
considered invalid.

### disk_lock_write:
```c
int disk_lock_write(bs_disk_t disk, void** data);
```
Lock `disk` for writing, returning a buffer which can be used to write data to the disk.

**Note**: Multiple threads may lock a disk for reading simultaneously, but only a single thread can lock a disk for writing at a time.

**Note**: Every successful call to `disk_lock_write` **must** be paired with a call to `disk_unlock_write`. Failure to do so can result in deadlock.

### disk_unlock_write:
```c
int disk_unlock_write(bs_disk_t disk);
```
Unlock a disk previously locked for writing on this thread.

**Note**: After calling this function, the buffer returned by `disk_lock_write` should be
considered invalid.


## Stego:
### Key Size:
```c
#define STEGO_KEY_BITS 128
```
Number of bits in a key; also the number of cover files present on the system.

### compute_level_size:
```c
size_t compute_level_size(size_t disk_size);
```
Calculate the size of a single level given the size of the disk.

### stego_read_level:
```c
int stego_read_level(const void* key, bs_disk_t disk,
  void* buf, off_t off, size_t size);
```
Read `size` bytes out of encrypted file specified by `key`, beginning at offset `off`.

*Assumes correctness of key.*

**Note:** Since the AES encryption is done cluster-wise, we have to make sure to read in clusters while using this function.

### stego_write_level:
```c
int stego_write_level(const void* key, bs_disk_t disk,
  const void* buf, off_t off, size_t size);
```
Write `size` bytes out of encrypted file specified by `key`, beginning at offset `off`.

**Note:** Since the AES encryption is done cluster-wise, we have to make sure to write in clusters while using this function.


### stego_gen_keys:
```c
int stego_gen_keys(void* buf, int count);
```
Generate the orthonormal extraction keys.

**Note**: This function is used only on initialization.


## AES:
### aes_encrypt:
```c
int aes_encrypt(const void* password, size_t password_size, const void* data, size_t size, void** buf_pointer, size_t* buf_size);
```
Encrpyt `size` bytes of `data` with 128-bit AES encryption using a key derived from `password`.<br>
Places the alocated result buffer of size `*buf_size` in `buf_pointer`.

**Note**: Make sure to free the buffer with `free` after use. 

### aes_decrypt:
```c
int aes_decrypt(const void* password, size_t password_size, const void* enc, size_t size, void** buf_pointer, size_t* buf_size);
```
Decrypt `size` bytes of `enc` with 128-bit AES decryption using a key derived from `password`.<br>
Places the alocated result buffer of size `*buf_size` in `buf_pointer`.

**Note**: Make sure to free the buffer with `free` after use. 

## Key Table:

### Max Levels:
```c
#define MAX_LEVELS 16
```
The maximum number of security levels on the system (and hence the maximal size
of the key table).

### Magic Number:
```c
#define KEYTAB_MAGIC 0xBEEFCAFE
```
This magic number appears at the beginning of every entry in the key table and is used to verify passwords.

### keytab_lookup:
```c
int keytab_lookup(bs_disk_t disk, const char* pass, void* key);
```
Verify `pass` against keytable using `KEYTAB_MAGIC`, and on success places key to level in `key`.

### keytab_store:
```c
int keytab_store(bs_disk_t disk, off_t index, const char* pass, const void* key);
```
Store `key`, together with `KEYTAB_MAGIC`, encrypted with `pass` at index `index` in the key table.

<hr>

# FUSE:
> Fuse requires being supplied a `fuse_operations` struct containing all the operations relevant to the file system. In our case every function pointer in that struct will point to a `bs_{function name}`  of the same signature.

## Cluster Management:

### Cluster Offsets:
```c
typedef uint32_t cluster_offset_t;
```
Represents a cluster offset.

```c
#define CLUSTER_OFFSET_EOF UINT32_MAX
```
Special offset value that indicates that a cluster is the last cluster in its file.

### Cluster Size:
```c
#define CLUSTER_SIZE 2048
```
The size of every cluster.
  

```c
#define CLUSTER_DATA_SIZE (CLUSTER_SIZE - sizeof(cluster_offset_t))
```
The effective size of every cluster, for data storage purposes (every cluster has a link to the next one at the end).

### fs_count_clusters:
```c
size_t fs_count_clusters(size_t level_size);
```
Determine how many clusters would fit into a level of size `level_size`, taking the BFT and allocation bitmap into account.


### fs_read_cluster:
```c
int fs_read_cluster(const void* key, bs_disk_t disk, void* buf,
  cluster_offset_t cluster);
```
Read the `cluster_index` cluster from the file matching `key` and places its contents in `buf`.

**Note:** Assumes `buf` is of size `CLUSTER_SIZE`.

### fs_write_cluster:
```c
int fs_write_cluster(const void* key, bs_disk_t disk,
  const void* buf, cluster_offset_t cluster);
```
Write the contents of `buf` to the cluster specified by `cluster`, in the level specified by `key`.

**Note:** Assumes `buf` is of size `CLUSTER_SIZE`.

### fs_next_cluster:
```c
cluster_offset_t fs_next_cluster(const void* cluster);
```
Find the index of the next cluster in the cluster chain (file).

**Note**: If `CLUSTER_OFFSET_EOF` is returned, `cluster` is the last cluster in the chain.

### fs_read_bitmap:
```c
int fs_read_bitmap(const void* key, bs_disk_t disk,
  void* buf, size_t bitmap_size);
```
Read the bitmap of the level matching `key` to `buf`.

### fs_write_bitmap:
```c
int fs_write_bitmap(const void* key, bs_disk_t disk,
  const void* buf, size_t bitmap_size);
```
Write the contents of `buf` to the bitmap in the level specified by `key`.

### fs_alloc_cluster:
```c
int fs_alloc_cluster(void* bitmap, cluster_offset_t* new_cluster);
```
Find the first empty cluster in `bitmap` and change its status bit to 1, meaning it is in use. 

### fs_dealloc_cluster:
```c
int fs_dealloc_cluster(void* bitmap, cluster_offset_t cluster);
```
Deallocate the cluster specified by `cluster` and change its status bit to 0, meaning it is free. 

## BFT:

### Timestamp:
```c
typedef uint32_t bft_timestamp_t;
```
Represents a file timestamp, stored in the BFT.

### BFT_MAX_ENTRIES:
```c
#define BFT_MAX_ENTRIES 8192
```
Maximum number of entries in the BFT.

### BFT_ENTRY_SIZE:
```c
#define BFT_ENTRY_SIZE 84
```
Size of a BFT entry on disk, in bytes.

### BFT_MAX_FILENAME:
```c
#define BFT_MAX_FILENAME 64
```
Maximum length of a file name, in bytes.

**Note**: This includes the terminating null character, capping the effective maximum file name at 63 bytes.


### bft_offset_t:
```c
typedef int16_t bft_offset_t;
```
Represents an offset into the BFT.

### struct bft_entry:
```c
typedef struct bft_entry {
  const char* name;
  cluster_offset_t initial_cluster;
  size_t size;
  mode_t mode;
  bft_timestamp_t atim;
  bft_timestamp_t mtim;
} bft_entry_t;
```

#### Structure on Disk (stored in big-endian):

1. `name` - 64 UTF-8 Code Units (including terminating null)
1. `initial_cluster` - 32 Bits
1. `size` - 32 Bits
1. `mode` - 32 Bits
1. `atim` - 32 Bits
1. `mtim` - 32 Bits

### bft_entry_init:
```c
int bft_entry_init(bft_entry_t* ent, const char* name, size_t size, mode_t mode,
  cluster_offset_t initial_cluster, bft_timestamp_t atim, bft_timestamp_t mtim);
```
Initializes a `bft_entry` with the specified information. `name` is copied into `ent->name`, so the original may be destroyed.

**Note**: When `ent` is no longer in use, be sure to call `bft_entry_destroy`.

### bft_entry_destroy:
```c
void bft_entry_destroy(bft_entry_t* ent);
```
Destroy the entry and deallocate any memory allocated by `bft_entry_init`. After calling this function, `ent` should be considered invalid and should not be used unless it is reinitialized.

### bft_find_free_table_entry:
```c
int bft_find_free_table_entry(const void* bft, bft_offset_t* off);
```
Search for empty space in `bft`, returning the offset of one of the available entries.

### bft_find_table_entry:
```c
int bft_find_table_entry(const void* bft, const char* filename, bft_offset_t* off);
```
Look up the file specified by `filename` in `bft`. If found, set `off` to the offset of the relevant entry.

**Note**: assumes that `bft` is a buffer of size `BFT_ENTRY_SIZE * BFT_MAX_ENTRIES`.

### bft_read_table_entry:
```c
int bft_read_table_entry(const void* bft, bft_entry_t* ent, bft_offset_t off);
```
Read the entry at offset `off` in `bft` and fill out `ent`.

**Note**: `ent` should be freed only if the function succeeds.

### bft_write_table_entry:
```c
int bft_write_table_entry(void* bft, const bft_entry_t* ent, bft_offset_t off);
```
Write `ent` to `bft` at offset `off`.

**Note**: The previous content at offset `off`, if any exists, is overwritten.
**Note**: Assumes that `bft` is a buffer of size `BFT_ENTRY_SIZE * BFT_MAX_ENTRIES`.

### bft_remove_table_entry:
```c
int bft_remove_table_entry(void* bft, bft_offset_t off);
```
Remove the entry at offset `off` from `bft`.

**Note**: assumes that `bft` is a buffer of size `BFT_ENTRY_SIZE * BFT_MAX_ENTRIES`.

### bft_entry_iter_t:
```c
typedef void (*bft_entry_iter_t)(bft_offset_t, const bft_entry_t*, void*);
```
A callback function which is called once for every entry in the BFT via
`bft_iter_table_entries`.

### bft_iter_table_entries:
```c
int bft_iter_table_entries(const void* bft, bft_entry_iter_t iter, void* ctx);
```
Iterate over all of the entries in `bft`, calling `iter` on each. `ctx` will be
passed directly to the function on every iteration and can be used to maintain
application-specific data.

**Note**: assumes that `bft` is a buffer of size `BFT_ENTRY_SIZE * BFT_MAX_ENTRIES`.

### bft_read_table:
```c
int bft_read_table(const void* key, bs_disk_t disk, void* bft);
```
Read the BFT written at the beginning of the level specified by `key` into `bft`.

**Note**: assumes that `bft` is a buffer of size `BFT_ENTRY_SIZE * BFT_MAX_ENTRIES`.

### bft_write_table:
```c
int bft_write_table(const void* key, bs_disk_t disk, const void* bft);
```
Write `bft` to the beginning of the level specified by `key`.

**Note**: assumes that `bft` is a buffer of size `BFT_ENTRY_SIZE * BFT_MAX_ENTRIES`.
