@echo off

mkdir build
pushd build
cl /Zi ..\src\gp_encoder.cc
popd
