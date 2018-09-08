FROM ubuntu:bionic

# Install Dependencies
RUN apt-get update 
RUN apt-get install -y clang check wget git libssl-dev cmake pkg-config meson ninja-build xz-utils

# Install libfuse
#RUN wget https://github.com/libfuse/libfuse/releases/download/fuse-3.2.6/fuse-3.2.6.tar.xz
#RUN tar xf fuse-3.2.6.tar.xz
#RUN tree
#RUN cd ./fuse-3.2.6
#RUN pwd
#RUN meson build
#RUN cd build
#RUN ninja
#RUN ninja install
#RUN ldconfig
#RUN cd ../..

RUN git clone https://github.com/ThatFatPat/BSFS /root/BSFS
RUN cd /root/BSFS
RUN ./install_fuse.sh