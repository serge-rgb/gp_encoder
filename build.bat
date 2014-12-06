@echo off

IF NOT EXIST build mkdir build
pushd build
cl /DTJE_STANDALONE /FC /Zi ..\src\gp_encoder.cc
popd
