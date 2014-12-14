@echo off

set warnings=/WX /W4 /wd4100 /wd4310
set defines=/D_CRT_SECURE_NO_WARNINGS /DTJE_STANDALONE /DTJE_DEBUG
set compiler_flags=/FC /Zi /Od /Oi
IF NOT EXIST build mkdir build
pushd build
cl /nologo /MP %warnings% %defines% %compiler_flags% ..\src\gp_encoder.cc
popd
