find_package(PkgConfig REQUIRED)
pkg_check_modules(FUSE3 REQUIRED fuse3)

set(FUSE_VERSION 31)

set(SOURCES fuse_ops.c main.c)

add_executable(bsfs ${SOURCES})
target_compile_definitions(bsfs PRIVATE FUSE_USE_VERSION=${FUSE_VERSION})

target_include_directories(bsfs SYSTEM PRIVATE ${FUSE3_INCLUDE_DIR})
target_link_libraries(bsfs PRIVATE bsfs-core ${FUSE3_LIBRARIES})
