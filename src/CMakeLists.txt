set(THREADS_PREFER_PTHREAD_FLAG ON)

find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)

include_directories(SYSTEM ${OPENSSL_INCLUDE_DIR})

set(SOURCES
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

add_library(bsfs-core ${SOURCES})
target_link_libraries(bsfs-core ${OPENSSL_LIBRARIES} Threads::Threads)

add_subdirectory(fuse)
