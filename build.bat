@echo off

mkdir build
pushd build
cl /FC /Zi ..\src\gp_encoder.cc
popd
