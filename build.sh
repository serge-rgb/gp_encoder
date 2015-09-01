#!/bin/sh

if [ ! -d build ]; then
    mkdir build
fi

clang++ -g -DTJE_DEBUG -I./src -I./third_party ./src/linux_evolve.cc -o build/evolve
