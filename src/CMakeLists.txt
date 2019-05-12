set(THREADS_PREFER_PTHREAD_FLAG ON)

find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)

add_library(bsfs-core "")
target_sources(bsfs-core PRIVATE
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

target_include_directories(bsfs-core SYSTEM PRIVATE ${OPENSSL_INCLUDE_DIR})
target_compile_options(bsfs-core PRIVATE ${OPENSSL_DEFINITIONS})

target_include_directories(bsfs-core INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(bsfs-core PRIVATE ${OPENSSL_LIBRARIES} PUBLIC Threads::Threads)

add_subdirectory(fuse)
