find_package(OpenSSL REQUIRED)

find_package(PkgConfig REQUIRED)
pkg_check_modules(FUSE3 REQUIRED fuse3)

include_directories(SYSTEM ${OPENSSL_INCLUDE_DIR} ${FUSE3_INCLUDE_DIR})

set(SOURCE_FILES
  bft.c
  enc.c
  fuse_ops.c
  keytab.c
  stego.c
  disk.c
)

add_library(bsfs-core ${SOURCE_FILES})
target_link_libraries(bsfs-core ${OPENSSL_LIBRARIES} ${FUSE3_LIBRARIES})

add_executable(bsfs main.c)
target_link_libraries(bsfs bsfs-core ${FUSE3_LIBRARIES})
