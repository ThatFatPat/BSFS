FROM ubuntu:bionic

# Install Dependencies
RUN apt-get update 
RUN apt-get install -y clang check wget libssl-dev cmake meson ninja-build xz-utils

# Install libfuse
RUN wget https://github.com/libfuse/libfuse/releases/download/fuse-3.2.6/fuse-3.2.6.tar.xz
RUN tar xf fuse-3.2.6.tar.xz
RUN cd fuse-3.2.6
RUN meson build
RUN cd build
RUN ninja
RUN ninja install
RUN ldconfig
RUN cd ../..

RUN git clone https://github.com/ThatFatPat/BSFS /root/BSFS
RUN cd /root/BSFS