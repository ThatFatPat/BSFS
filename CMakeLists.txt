cmake_minimum_required(VERSION 3.9)
project(BSFS C)

include(CheckIPOSupported)
include(CheckCCompilerFlag)

check_ipo_supported(RESULT IPO_SUPPORTED OUTPUT IPO_OUTPUT)
if (IPO_SUPPORTED)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
else()
    message(WARNING "IPO is not supported: ${IPO_OUTPUT}")
endif()

add_compile_options(-Wall -Wextra -Wpedantic)

if (DEFINED OPTIMIZE_FOR_ARCH)
    check_c_compiler_flag(-march=${OPTIMIZE_FOR_ARCH} MARCH_SUPPORTED)
    if (MARCH_SUPPORTED)
        add_compile_options(-march=${OPTIMIZE_FOR_ARCH})
    else()
        message(WARNING "unsupported target architecture")
    endif()
endif()

add_subdirectory(src)
add_subdirectory(tests)

enable_testing()
add_test(test tests/bsfs-test)
