FROM ubuntu:bionic

# Install Dependencies
RUN sudo apt-get update 
RUN sudo apt-get install -y --force-yes clang check libssl-dev cmake meson ninja-build build-essential xz-utils

# Install libfuse
RUN wget https://api.github.com/repos/libfuse/libfuse/tarball/fuse-3.2.6
RUN tar xf fuse-3.2.6
RUN cd libfuse-libfuse-569e6ba
RUN meson build
RUN cd build
RUN ninja
RUN sudo ninja install
RUN sudo ldconfig

RUN git clone https://github.com/ThatFatPat/BSFS /root/BSFS
RUN cd /root/BSFS