find_package(PkgConfig QUIET REQUIRED)
pkg_check_modules(Fuse3 REQUIRED fuse3 IMPORTED_TARGET)

set(FUSE_API_VERSION 31)

add_executable(bsfs "")
target_sources(bsfs PRIVATE fuse_ops.c main.c)

target_compile_definitions(bsfs PRIVATE FUSE_USE_VERSION=${FUSE_API_VERSION})

target_link_libraries(bsfs PRIVATE bsfs-core PkgConfig::Fuse3)
