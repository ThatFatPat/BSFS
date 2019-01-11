FROM ubuntu:bionic

# Install Dependencies
RUN apt-get update && apt-get install -y clang check wget git libssl-dev cmake pkg-config meson udev ninja-build xz-utils

# Install libfuse
RUN wget https://github.com/libfuse/libfuse/releases/download/fuse-3.2.6/fuse-3.2.6.tar.xz
RUN tar xf fuse-3.2.6.tar.xz
RUN cd ./fuse-3.2.6 && meson build && cd build && ninja && ninja install && ldconfig

RUN git clone https://github.com/ThatFatPat/BSFS /root/BSFS