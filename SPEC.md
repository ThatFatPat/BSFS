# API:
> **All functions should return a (negative) error code on failure, 0 on success.**

## Stego:
### Key Size:
```c
#define STEGO_KEY_SIZE 128
```
This key size is also the number of cover files present on the system.


### stego_read_level:
```c
int stego_read_level(const void* key, const void* disk, size_t level_size,
  void* buf, off_t off, size_t size);
```
Read `size` bytes out of encrypted file specified by `key`, beginning at offset `off`.

*Assumes correctness of key.*

**Note:** Since the AES encryption is done cluster-wise, we have to make sure to read in clusters while using this function.

### stego_write_level:
```c
int stego_write_level(const void* key, void* disk, size_t level_size,
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
int aes_encrypt(const void* key, const void* data, size_t size, void* buf);
```
Encrpyt `size` bytes of `data` with 128-bit AES encryption using `key`.<br>
Places the result in `buf`.

### aes_decrypt:
```c
int aes_decrypt(const void* key, const void* enc, size_t size, void* buf);
```
Decrypt `size` bytes of `enc` with 128-bit AES decryption using `key`.<br>
Places the result in `buf`.


## Key Table:

### Max Levels:
```c
#define MAX_LEVELS 16
```

### Magic Number:
```c
#define KEYTAB_MAGIC 0xBEEFCAFE
```
This magic number appears at the beginning of every entry in the key table and is used to verify passwords.

### keytab_lookup:
```c
int keytab_lookup(const void* disk, const void* pass, void* key);
```
Verify `pass` against keytable using `KEYTAB_MAGIC`, and on success places key to level in `key`.

### keytab_store:
```c
int keytab_store(const void* disk, off_t index, const void* pass, const void* key);
```
Store `key`, together with `KEYTAB_MAGIC`, encrypted with `pass` at index `index` in the key table.

<hr>

# FUSE:
> Fuse requires being supplied a `fuse_operations` struct containing all the operations relevant to the file system. In our case every function pointer in that struct will point to a `bs_{function name}`  of the same signature.

### compute_level_size
```c
size_t compute_level_size(size_t disk_size);
```
Calculate the size of a single level given the size of the disk.

## Cluster Management

### Cluster Offsets
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

### fs_count_clusters
```c
size_t fs_count_clusters(size_t level_size);
```
Determine how many clusters would fit into a level of size `level_size`, taking the BFT and allocation bitmap into account.


### fs_read_cluster:
```c
int fs_read_cluster(const void* key, const void* disk, size_t level_size,
  void* buf, cluster_offset_t cluster);
```
Read the `cluster_index` cluster from the file matching `key` and places its contents in `buf`.

**Note:** Assumes `buf` is of size `CLUSTER_SIZE`.

### fs_write_cluster:
```c
int fs_write_cluster(const void* key, const void* disk, size_t level_size,
  const void* buf, cluster_offset_t cluster);
```
Write the contents of `buf` to the cluster specified by `cluster`, in the level specified by `key`.

**Note:** Assumes `buf` is of size `CLUSTER_SIZE`.

### fs_next_cluster
```c
cluster_offset_t fs_next_cluster(const void* cluster);
```
Find the index of the next cluster in the cluster chain (file).

**Note**: If `CLUSTER_OFFSET_EOF` is returned, `cluster` is the last cluster in the chain.

### fs_read_bitmap


### fs_alloc_cluster
```c
int fs_alloc_cluster(void* bitmap, cluster_offset_t* new_cluster);
```
Find the first empty cluster in `bitmap` and change its status bit to 1, meaning it is in use. 

### fs_dealloc_cluster
```c
int fs_dealloc_cluster(void* bitmap, cluster_offset_t cluster);
```
Deallocate the cluster specified by `cluster` and change its status bit to 0, meaning it is free. 

## BFT

### Timestamp
```c
typedef uint32_t bft_timestamp_t;
```
Represents a file timestamp, stored in the BFT.

### BFT_MAX_ENTRIES
```c
#define BFT_MAX_ENTRIES 8192
```
Maximum number of entries in the BFT.

### BFT_ENTRY_SIZE
```c
#define BFT_ENTRY_SIZE 84
```
Size of a BFT entry on disk, in bytes.

### BFT_MAX_FILENAME
```c
#define BFT_MAX_FILENAME 64
```
Maximum length of a file name, in bytes.


### struct bft_entry
```c
typedef struct bft_entry {
  const char* name;
  cluster_offset_t initial_cluster;
  size_t size;
  mode_t mode;
  bft_timestamp_t atim;
  bft_timestamp_t mtim;
} bft_entry;
```

#### Structure on Disk (stored in big-endian):

1. `name` - 64 UTF-8 Code Units
1. `initial_cluster` - 32 Bits
1. `size` - 32 Bits
1. `mode` - 32 Bits
1. `atim` - 32 Bits
1. `mtim` - 32 Bits

### bft_entry_init
```c
int bft_entry_init(bft_entry* ent, const char* name, size_t size, mode_t mode,
  cluster_offset_t initial_cluster, bft_timestamp_t atim, bft_timestamp_t mtim);
```
Initializes a `bft_entry` with the specified information. `name` is copied into `ent->name`, so the original may be destroyed.

**Note**: When `ent` is no longer in use, be sure to call `bft_entry_destroy`.

### bft_entry_destroy
```c
void bft_entry_destroy(bft_entry* ent);
```
Destroy the entry and deallocate any memory allocated by `bft_entry_init`. After calling this function, `ent` should be considered invalid and should not be used unless it is reinitialized.

### bft_lookup:
```c
int bft_lookup(const void* bft, const char* filename, bft_entry* ent);
```
Look up the file specified by `filename` in `bft`. If found, fill out the respective fields of `ent` to contain information about the requested entry.

**Note**: `ent` should be passed uninitialized, and should be destroyed with `bft_entry_destroy` only if the function succeeds.

### bft_write_entry
```c
int bft_write_entry(void* bft, const bft_entry* ent);
```
Write `ent` to the BFT. If an entry with the filename already exists, it is updated.

### bft_remove_entry
```c
int bft_remove_entry(void* bft, const bft_entry* ent);
```
Remove the entry indicated by `ent` from `bft`.
