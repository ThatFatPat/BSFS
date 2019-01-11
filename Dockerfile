FROM nmraz/fuse3-ubuntu-dev:latest

COPY . /bsfs
WORKDIR /bsfs

CMD [ "/bin/bash", "-c", "mkdir build && cd build && CC=clang cmake -G Ninja .. && cmake --build . && ctest -V" ]