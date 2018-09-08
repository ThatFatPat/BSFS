FROM ubuntu:bionic

# Install Dependencies
RUN apt-get update 
RUN apt-get install -y clang check wget libssl-dev cmake meson ninja-build xz-utils

# Install libfuse
RUN wget https://api.github.com/repos/libfuse/libfuse/tarball/fuse-3.2.6
RUN tar xf fuse-3.2.6
RUN cd libfuse-libfuse-569e6ba
RUN meson build
RUN cd build
RUN ninja
RUN ninja install
RUN ldconfig
RUN cd ../..

RUN git clone https://github.com/ThatFatPat/BSFS /root/BSFS
RUN cd /root/BSFS