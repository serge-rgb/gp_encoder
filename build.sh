#!/bin/sh

if [ ! -d build ]; then
    mkdir build
fi

clang++ -g -DTJE_DEBUG -I./src ./src/evolve.cc -o build/evolve
