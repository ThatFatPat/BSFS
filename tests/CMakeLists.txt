find_package(PkgConfig QUIET REQUIRED)
pkg_check_modules(Check REQUIRED check IMPORTED_TARGET)

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

target_link_libraries(bsfs-test PRIVATE bsfs-core PkgConfig::Check)
