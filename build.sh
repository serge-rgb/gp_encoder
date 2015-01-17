#!/bin/sh

if [ ! -d build ]; then
    mkdir build
fi

pushd build
#clang++ -g -DTJE_STANDALONE -DTJE_DEBUG -Wno-null-dereference ../src/tiny_jpeg.cc
clang++ -g -DTJE_DEBUG -I../src ../src/evolve.cc
popd

