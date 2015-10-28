#!/bin/sh

if [ ! -d build ]; then
    mkdir build
fi

clang -g -DTJE_DEBUG \
    -I./src -I./third_party -I./src/tiny_jpeg -I./src/libserg \
    ./src/jpeg_test.c \
    -lpthread \
    -O2 --std=c99 \
    -o build/evolve
