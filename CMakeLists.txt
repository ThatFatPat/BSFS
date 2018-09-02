cmake_minimum_required(VERSION 3.0)
project(BSFS C)

find_package(OpenSSL REQUIRED)

find_package(PkgConfig REQUIRED)
pkg_check_modules(FUSE3 REQUIRED fuse3)

include_directories(SYSTEM ${OPENSSL_INCLUDE_DIR} ${FUSE3_INCLUDE_DIR})

set(SOURCE_FILES
  src/bft.c
  src/enc.c
  src/fuse_ops.c
  src/keytab.c
  src/stego.c
)

set(WARNINGS -Wall -Wextra)

add_library(bsfs-core ${SOURCE_FILES})
target_link_libraries(bsfs-core ${OPENSSL_LIBRARIES} ${FUSE3_LIBRARIES})
target_compile_options(bsfs-core PRIVATE ${WARNINGS})

add_executable(bsfs src/main.c)
target_link_libraries(bsfs bsfs-core ${FUSE3_LIBRARIES})
target_compile_options(bsfs PRIVATE ${WARNINGS})