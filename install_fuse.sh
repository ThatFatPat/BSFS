#!/bin/sh
curl https://github.com/libfuse/libfuse/releases/download/fuse-3.2.5/fuse-3.2.5.tar.xz
tar xf fuse-3.2.5.tar.xz
cd fuse-3.2.5
meson
ninja build
sudo ninja install