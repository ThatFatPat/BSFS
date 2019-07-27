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
### Amount of Cover Files:
```c
#define STEGO_COVER_FILE_COUNT 128
```
Number of cover files present on the system; also the number of bits in a key.

### Levels per Password
```c
#define STEGO_LEVELS_PER_PASSWORD 8
```
The number of internal stego levels per user password.

### Number of User Levels
```c
#define STEGO_USER_LEVEL_COUNT (STEGO_COVER_FILE_COUNT / STEGO_LEVELS_PER_PASSWORD)
```
Number of user levels per disk.

### Key Size:
```c
#define STEGO_KEY_SIZE (COVER_FILE_COUNT/CHAR_BIT)
```
The size of a level key, in bytes.

### AES Key Size
```c
#define STEGO_AES_KEY_SIZE 16
```
The size of a stego AES key.

### Stego Key
```c
typedef struct {
  uint8_t aes_key[STEGO_AES_KEY_SIZE];
  uint8_t read_keys[STEGO_LEVELS_PER_PASSWORD][STEGO_KEY_SIZE];
  uint8_t write_keys[STEGO_LEVELS_PER_PASSWORD][STEGO_KEY_SIZE];
} stego_key_t;
```
Represents a user level key.

#### Structure on Disk

1. `aes_key`
1. `read_keys`
1. `write_keys`

### stego_compute_user_level_size:
```c
size_t stego_compute_user_level_size(size_t disk_size);
```
Calculate the size of a user level given the size of the disk.

### stego_read_level:
```c
int stego_read_level(const stego_key_t* key, bs_disk_t disk,
  void* buf, off_t off, size_t size);
```
Read `size` bytes out of encrypted file specified by `key`, beginning at offset `off`.

*Assumes correctness of key.*

**Note:** Since the AES encryption is done cluster-wise, we have to make sure to read in clusters while using this function.

### stego_write_level:
```c
int stego_write_level(const stego_key_t* key, bs_disk_t disk,
  const void* buf, off_t off, size_t size);
```
Write `size` bytes out of encrypted file specified by `key`, beginning at offset `off`.

**Note:** Since the AES encryption is done cluster-wise, we have to make sure to write in clusters while using this function.

### stego_gen_user_keys
```c
int stego_gen_user_keys(stego_key_t* keys, size_t count);
```
Generate `count` user keys, placing the result in `keys`.


## AES:
### aes_encrypt:
```c
int aes_encrypt(const void* password, size_t password_size
  const void* salt, size_t salt_size, const void* plain, void* enc, size_t size);
```
Encrypt `size` bytes of `plain` with 128-bit AES encryption using a key derived from `password`
and `salt`.<br>
Places the encrypted result in `enc`.

**Note**: This function will fail if `size` is not a multiple of 16.

### aes_decrypt:
```c
int aes_decrypt(const void* password, size_t password_size,
  const void* salt, size_t salt_size, const void* enc, void* plain, size_t size);
```
Decrypt `size` bytes of `enc` with 128-bit AES decryption using a key derived from `password`
and `salt`.<br>
Places the decrypted result in `plain`.

**Note**: This function will fail if `size` is not a multiple of 16.

### aes_encrypt_auth:
```c
int aes_encrypt_auth(const void* password, size_t password_size,
  const void* salt, size_t salt_size, const void* plain,
  void* enc, size_t size, void* tag, size_t tag_size);
```
Authenticated encryption &mdash; encrypt `plain` with a key derived from `password` and `salt`, and generate a tag which can be used to verify the integrity of the data upon decryption. `salt` can be well-known (public) data (preferably random),
which will be used to protect key generation.

**Warning**: Do not store several pieces of data encrypted with the same password/salt **pair** in this mode.
Doing so could allow the data to be recovered.

### aes_decrypt_auth
```c
int aes_decrypt_auth(const void* password, size_t password_size,
  const void* salt, size_t salt_size, const void* enc, void* plain,
  size_t size, const void* tag, size_t tag_size);
```
Authenticated decryption &mdash; decrypt `enc` with `password` and `salt` in a manner complemetary to `aes_encrypt_auth`, and verify the data against `tag`.

**Note**: This function will fail with `EBADMSG` if the tag does not match, indicating that the data is corrupted or may have been tampered with.

## Key Table:

### Key Table Tag Size
```c
#define KEYTAB_TAG_SIZE 16
```
The size of a key table authentication tag.

### Key Table Entry Size:
```c
#define KEYTAB_ENTRY_SIZE                                                     \ 
  (STEGO_AES_KEY_SIZE + STEGO_LEVELS_PER_PASSWORD * STEGO_KEY_SIZE * 2 +          \
   KEYTAB_TAG_SIZE)
```
The size of a keytab entry in bytes.

### Key Table Salt Size
```c
#define KEYTAB_SALT_SIZE 16
```
The size of the salt used by the key table, stored at the beginning of the disk.

### Key Table Size
```c
#define KEYTAB_SIZE (KEYTAB_SALT_SIZE + KEYTAB_ENTRY_SIZE * STEGO_USER_LEVEL_COUNT)
```
The total size the keytable takes on disk.

### keytab_lookup:
```c
int keytab_lookup(bs_disk_t disk, const char* password, stego_key_t* key);
```
Look up `password` in the key table stored at the beginning of disk, and if found,
return the matching key that was stored there.

### keytab_store:
```c
int keytab_store(bs_disk_t disk, off_t index, const char* password, const stego_key_t* key);
```
Store `key`, authenticated and encrypted with `password` at index `index` in the key table.

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
int fs_read_cluster(const stego_key_t* key, bs_disk_t disk, void* buf,
  cluster_offset_t cluster);
```
Read the `cluster_index` cluster from the file matching `key` and places its contents in `buf`.

**Note:** Assumes `buf` is of size `CLUSTER_SIZE`.

### fs_write_cluster:
```c
int fs_write_cluster(const stego_key_t* key, bs_disk_t disk,
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
int fs_read_bitmap(const stego_key_t* key, bs_disk_t disk,
  void* buf, size_t bitmap_size);
```
Read the bitmap of the level matching `key` to `buf`.

### fs_write_bitmap:
```c
int fs_write_bitmap(const stego_key_t* key, bs_disk_t disk,
  const void* buf, size_t bitmap_size);
```
Write the contents of `buf` to the bitmap in the level specified by `key`.

### fs_alloc_cluster:
```c
int fs_alloc_cluster(void* bitmap, size_t bitmap_bits, cluster_offset_t start, cluster_offset_t* new_cluster);
```
Find the first empty cluster after `start` in `bitmap` and change its status bit to 1, meaning it is in use. 

### fs_dealloc_cluster:
```c
int fs_dealloc_cluster(void* bitmap, size_t bitmap_bits, cluster_offset_t cluster);
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

### BFT_SIZE
```c
#define BFT_SIZE (BFT_ENTRY_SIZE * BFT_MAX_ENTRIES)
```
Total size of the BFT on disk.

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

**Note**: assumes that `bft` is a buffer of size `BFT_SIZE`.

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

**Note**: Assumes that `bft` is a buffer of size `BFT_SIZE`.

### bft_remove_table_entry:
```c
int bft_remove_table_entry(void* bft, bft_offset_t off);
```
Remove the entry at offset `off` from `bft`.

**Note**: assumes that `bft` is a buffer of size `BFT_SIZE`.

### bft_entry_iter_t:
```c
typedef bool (*bft_entry_iter_t)(bft_offset_t, const bft_entry_t*, void*);
```
A callback function which is called once for every entry in the BFT via
`bft_iter_table_entries`.
Returning `false` indicates that iteration should stop.

### bft_iter_table_entries:
```c
int bft_iter_table_entries(const void* bft, bft_entry_iter_t iter, void* ctx);
```
Iterate over all of the entries in `bft`, calling `iter` on each. If `iter`
returns `false`, iteration stops. `ctx` will be passed directly to the function
on every iteration and can be used to maintain application-specific data.

**Note**: assumes that `bft` is a buffer of size `BFT_SIZE`.

### bft_read_table:
```c
int bft_read_table(const stego_key_t* key, bs_disk_t disk, void* bft);
```
Read the BFT written at the beginning of the level specified by `key` into `bft`.

**Note**: assumes that `bft` is a buffer of size `BFT_SIZE`.

### bft_write_table:
```c
int bft_write_table(const stego_key_t* key, bs_disk_t disk, const void* bft);
```
Write `bft` to the beginning of the level specified by `key`.

**Note**: assumes that `bft` is a buffer of size `BFT_SIZE`.
