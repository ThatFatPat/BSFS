#!/bin/sh
wget https://github.com/libfuse/libfuse/releases/download/fuse-3.2.6/fuse-3.2.6.tar.xz
tar xf fuse-3.2.6.tar.xz
cd fuse-3.2.6
meson build
cd build
ninja
sudo ninja install