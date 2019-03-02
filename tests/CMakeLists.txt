find_package(PkgConfig REQUIRED)
pkg_check_modules(CHECK REQUIRED check)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../src/ ${CHECK_INCLUDE_DIR})

set(TEST_SOURCES
  test_bft.c
  test_bit_util.c
  test_bsfs.c
  test_cluster.c
  test_disk.c
  test_enc.c
  test_keytab.c
  test_matrix.c
  test_stego.c
  test_vector.c
  test_cluster.c
  main.c
)

add_executable(bsfs-test ${TEST_SOURCES})
target_link_libraries(bsfs-test bsfs-core ${CHECK_LIBRARIES})
