FROM nmraz/bsfs-build-env:latest

COPY . /bsfs
WORKDIR /bsfs

CMD [ "/bin/bash", "-c", "mkdir build && cd build && CC=clang cmake -DCMAKE_BUILD_TYPE=Release -G Ninja .. && cmake --build . && ctest -V" ]
