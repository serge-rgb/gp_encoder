#!/bin/sh

if [ ! -d build ]; then
    mkdir build
fi

pushd build
clang++ -g -DTJE_STANDALONE ../src/gp_encoder.cc
popd

