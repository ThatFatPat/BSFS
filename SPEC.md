# API:
## Stego:
### Key Size:
```c
#define STEGO_KEY_SIZE 128
```


### stego_read_level:
```c
int stego_read_level(const void* key, const void* disk, size_t disk_size,
  void* buf, off_t off, size_t size);
```
Read `size` bytes out of encrypted file specified by `key`, beginning at offset `off`.
<br />
*Assumes correctness of key.*

<br>**Note:** Since the AES encryption is done cluster-wise, we have to make sure to read in clusters while using this function.

### stego_write_level:
```c
int stego_write_level(const void* key, void* disk, size_t disk_size,
  const void* buf, off_t off, size_t size);
```
<br>**Note:** Since the AES encryption is done cluster-wise, we have to make sure to read in clusters while using this function.



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


## Fuse:

### read_cluster:
```c
int read_cluster(const void* key, const void* disk, size_t disk_size,
  void* buf, off_t cluster)
```
Reads cluster `cluster` from the file matching `key` and places its contents in `buf`.

### write_cluster:
```c
int write_cluster(const void* key, const void* disk, size_t disk_size,
  void* content, off_t cluster)
```
