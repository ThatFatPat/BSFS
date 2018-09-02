cmake_minimum_required(VERSION 3.0)
project(BSFS)

find_package(OpenSSL REQUIRED)

find_package(PkgConfig REQUIRED)
pkg_check_modules(FUSE3 REQUIRED fuse3)

include_directories(SYSTEM ${OPENSSL_INCLUDE_DIR} ${FUSE3_INCLUDE_DIR})

set(SOURCE_FILES
  src/bft.c
  src/bs_aes.c
  src/fuse_ops.c
  src/keytab.c
  src/stego.c
  src/main.c
)

add_executable(bsfs ${SOURCE_FILES})
target_link_libraries(bsfs ${OPENSSL_LIBRARIES} ${FUSE3_LIBRARIES})