cmake_minimum_required(VERSION 3.0)
project(BSFS C)

add_subdirectory(src)
add_subdirectory(tests)

enable_testing()
add_test(test tests/bsfs-test)