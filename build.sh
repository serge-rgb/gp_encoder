#!/bin/sh

if [ ! -d build ]; then
    mkdir build
fi

clang -g -DTJE_DEBUG \
    -Wall -Wextra \
    -Wno-missing-braces -Wno-incompatible-pointer-types-discards-qualifiers -Wno-missing-field-initializers \
    -I./src -I./third_party -I./src/tiny_jpeg -I./src/libserg \
    ./src/evolve_CPU.c \
    -lpthread \
    -O2 --std=c99 \
    -framework OpenCL \
    -o build/evolve

