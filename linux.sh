#!/bin/sh

if [ ! -d build ]; then
    mkdir build
fi

clang -g -DTJE_DEBUG \
    -D_BSD_SOURCE \
    -Wall -Wextra \
    -Wno-missing-braces -Wno-incompatible-pointer-types-discards-qualifiers -Wno-missing-field-initializers \
    -I./src -I./third_party -I./src/tiny_jpeg -I./src/libserg \
    ./src/extended_jpeg.c \
    ./src/evolve_CPU.c \
    -O0 -g --std=gnu99 \
    -lpthread \
    -lOpenCL \
    -lm \
    -lrt \
    -o build/evolve
