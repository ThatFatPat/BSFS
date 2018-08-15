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
<br />
*Assumes correctness of key.*

**Note:** Since the AES encryption is done cluster-wise, we have to make sure to read in clusters while using this function.

### stego_write_level:
```c
int stego_write_level(const void* key, void* disk, size_t level_size,
  const void* buf, off_t off, size_t size);
```
Write `size` bytes out of encrypted file specified by `key`, beginning at offset `off`.
<br>**Note:** Since the AES encryption is done cluster-wise, we have to make sure to write in clusters while using this function.



### stego_gen_keys:
```c
int stego_gen_keys(void* buf, int count);
```
Generate the orthonormal extraction keys.<br>
Will be called at initializion.


## AES:
### aes_encrypt:
```c
int aes_encrypt(const void* key, const void* data, size_t size,  void* buf);
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

### Magic Number:
```c
#define KEYTAB_MAGIC 0xBEEFCAFE
```
This magic number appears at the beginning of every entry in the key table and is used to verify passwords.

### keytab_lookup:
```c
int keytab_lookup(const void* pass, void* key);
```
Verifies `pass` against keytable using `KEYTAB_MAGIC`, and on success places key to level in `key`.<br>

### keytab_store:
```c
int keytab_store(off_t index, const void* pass, const void* key);
```
Stores `key`, together with `KEYTAB_MAGIC`, encrypted with `pass` at index `index` in the key table.

<br><br>

# FUSE:
> Fuse requires being supplied a `fuse_operations` struct containing all the operations relevant to the file system. In our case every function pointer in that struct will point to a `bs_{function name}`  of the same signature.

## Cluster Management

### CLUSTER_SIZE:
```c
#define CLUSTER_SIZE 2048
```

### fs_count_clusters
```c
size_t fs_count_clusters(size_t level_size);
```
Determine how many clusters would fit into a level of size `level_size`, taking the BFT and allocation bitmap into account.


### fs_read_cluster:
```c
int fs_read_cluster(const void* key, const void* disk, size_t level_size,
  void* buf, off_t cluster_index);
```
Read the `cluster_index` cluster from the file matching `key` and places its contents in `buf`.<br>
**Note:** Assumes `buf` is of size `CLUSTER_SIZE`.

### fs_write_cluster:
```c
int fs_write_cluster(const void* key, const void* disk, size_t level_size,
  const void* content, off_t cluster_index);
```
Write the `cluster_index` cluster to the file matching `key` to be the contents in `buf`.<br>
**Note:** Assumes `buf` is of size `CLUSTER_SIZE`.

## BFT

### struct bft_entry
```c
struct bft_entry {

}
```

### fs_bft_lookup:
```c
int fs_bft_lookup(const void* bft, const char* filename);
```
Look up the file specified by `filename` in `bft`. On success, return the index of the initial cluster of the file, and on failure return an error code (ENOENT).