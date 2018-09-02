cmake_minimum_required(VERSION 3.0)
project(BSFS)

set(SOURCE_FILES
  src/bft.c
  src/bs_aes.c
  src/bs_fuse.c
  src/keytab.c
  src/stego.c
  src/main.c
)

add_executable(bsfs ${SOURCE_FILES})