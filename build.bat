@echo off

IF NOT EXIST build mkdir build
pushd build
cl /nologo /WX /W4 /wd4100 /wd4310 /D_CRT_SECURE_NO_WARNINGS /DTJE_STANDALONE /FC /Zi ..\src\gp_encoder.cc
popd
