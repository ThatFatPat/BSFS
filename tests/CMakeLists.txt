find_package(PkgConfig REQUIRED)
pkg_check_modules(CHECK REQUIRED check)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../src/ ${CHECK_INCLUDE_DIR})

set(TEST_SOURCES
  test_bft.c
  test_enc.c
  test_fuse_ops.c
  test_keytab.c
  test_stego.c
  main.c
)

add_executable(bsfs-test ${TEST_SOURCES})
target_link_libraries(bsfs-test bsfs-core ${CHECK_LIBRARIES})
