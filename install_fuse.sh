#!/bin/sh
wget https://api.github.com/repos/libfuse/libfuse/tarball/fuse-3.2.6
tar xf fuse-3.2.6
cd libfuse-libfuse-569e6ba
meson build
cd build
ninja
sudo ninja install