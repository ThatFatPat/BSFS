FROM nmraz/bsfs-build-env:latest

COPY . /bsfs
WORKDIR /bsfs

CMD [ "/bin/bash", "-c", "mkdir build && cd build && CC=clang cmake -G Ninja .. && cmake --build . && ctest -V" ]