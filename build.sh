#!/bin/sh

if [ ! -d build ]; then
    mkdir build
fi

clang++ -g -DTJE_DEBUG -I./src ./src/osx_evolve.cc -o build/evolve
