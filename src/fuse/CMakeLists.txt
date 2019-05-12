find_package(PkgConfig REQUIRED)
pkg_check_modules(FUSE3 REQUIRED fuse3)

include_directories(SYSTEM ${FUSE3_INCLUDE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/..)

set(SOURCES fuse_ops.c main.c)

add_executable(bsfs ${SOURCES})
target_link_libraries(bsfs bsfs-core ${FUSE3_LIBRARIES})
