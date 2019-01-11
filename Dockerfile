FROM ubuntu:cosmic

# Install Dependencies
RUN apt-get update && apt-get install -y clang check wget git libssl-dev cmake pkg-config meson udev ninja-build xz-utils

# Install libfuse
RUN wget https://github.com/libfuse/libfuse/releases/download/fuse-3.4.1/fuse-3.4.1.tar.xz
RUN tar xf fuse-3.4.1.tar.xz
RUN cd ./fuse-3.4.1 && meson build && cd build && ninja && ninja install && ldconfig

COPY . /bsfs
WORKDIR /bsfs

CMD [ "/bin/bash", "-c", "mkdir build && cd build && CC=clang cmake -G Ninja .. && cmake --build . && ctest -V" ]