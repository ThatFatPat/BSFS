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
int stego_read_level(const void* key, const void* disk, size_t disk_size,
  void* buf, off_t off, size_t size);
```
Read `size` bytes out of encrypted file specified by `key`, beginning at offset `off`.
<br />
*Assumes correctness of key.*

**Note:** Since the AES encryption is done cluster-wise, we have to make sure to read in clusters while using this function.

### stego_write_level:
```c
int stego_write_level(const void* key, void* disk, size_t disk_size,
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
int aes_encrypt(const void* key, const void* data, size_t size,  void* buf)
```
Encrpyt `size` bytes of `data` with 128-bit AES encryption using `key`.<br>
Places the result in `buf`.

### aes_decrypt:
```c
int aes_decrypt(const void* key, const void* enc, size_t size, void* buf)
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
int keytab_lookup(const void* pass, void* key)
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

### CLUSTER_SIZE:
```c
#define CLUSTER_SIZE 2048
```


### read_cluster:
```c
int read_cluster(const void* key, const void* disk, size_t disk_size,
  void* buf, off_t cluster_index)
```
Read the `cluster_index` cluster from the file matching `key` and places its contents in `buf`.<br>
**Note:** Assumes `buf` is of size `CLUSTER_SIZE`.

### write_cluster:
```c
int write_cluster(const void* key, const void* disk, size_t disk_size,
  const void* content, off_t cluster_index)
```
Write the `cluster_index` cluster to the file matching `key` to be the contents in `buf`.<br>
**Note:** Assumes `buf` is of size `CLUSTER_SIZE`.