find_package(OpenSSL REQUIRED)

include_directories(SYSTEM ${OPENSSL_INCLUDE_DIR})

set(SOURCE_FILES
  bft.c
  bit_util.c
  bsfs.c
  cluster.c
  disk.c
  enc.c
  keytab.c
  matrix.c
  oft.c
  stego.c
  vector.c
)

add_library(bsfs-core ${SOURCE_FILES})
target_link_libraries(bsfs-core ${OPENSSL_LIBRARIES} pthread)

add_subdirectory(fuse)
