find_package(PkgConfig REQUIRED)
pkg_check_modules(CHECK REQUIRED check)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../src/ ${CHECK_INCLUDE_DIR})

set(TEST_SOURCES
  test_bft.c
  test_bft.h
  test_bit_util.c
  test_bit_util.h
  test_bsfs.c
  test_bsfs.h
  test_cluster.c
  test_cluster.h
  test_disk.c
  test_disk.h
  test_enc.c
  test_enc.h
  test_keytab.c
  test_keytab.h
  test_stego.c
  test_stego.h
  test_vector.c
  main.c
)

add_executable(bsfs-test ${TEST_SOURCES})
target_link_libraries(bsfs-test bsfs-core ${CHECK_LIBRARIES})
