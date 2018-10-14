cmake_minimum_required(VERSION 3.9)
project(BSFS C)

include(CheckIPOSupported)

check_ipo_supported(RESULT ipo_supported OUTPUT ipo_output)
if (ipo_supported)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
else()
    message(WARNING "IPO is not supported: ${ipo_output}")
endif()

add_compile_options(-Wall -Wextra -Wpedantic)

add_subdirectory(src)
add_subdirectory(tests)

enable_testing()
add_test(test tests/bsfs-test)