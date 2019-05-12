find_package(PkgConfig REQUIRED)
pkg_check_modules(CHECK REQUIRED check)

add_executable(bsfs-test "")
target_sources(bsfs-test PRIVATE
  test_bft.c
  test_bit_util.c
  test_bsfs.c
  test_cluster.c
  test_disk.c
  test_enc.c
  test_keytab.c
  test_matrix.c
  test_oft.c
  test_stego.c
  test_vector.c
  test_cluster.c
  main.c
)

target_include_directories(bsfs-test SYSTEM PRIVATE ${CHECK_INCLUDE_DIR})
target_compile_options(bsfs-test PRIVATE ${CHECK_CFLAGS_OTHER})
target_link_libraries(bsfs-test PRIVATE bsfs-core ${CHECK_LIBRARIES})
