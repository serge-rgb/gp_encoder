@echo off

set warnings=/WX /W4 /wd4100 /wd4310 /wd4189
:: warning 4189 is useful for polisihing code, but it's a pain while developing.
set warnings_release=%warnings% /wd4189

set defines=/D_CRT_SECURE_NO_WARNINGS /DTJE_DEBUG
set compiler_flags=/FC /Zi /I../src
set compiler_flags_debug=%compiler_flags% %warnings% /Od /Oi
set compiler_flags_release=%compiler_flags% %warnings_release% /O2
:: set linker_flags= /opt:ref

IF NOT EXIST build mkdir build
pushd build
cl /nologo /MP %warnings% %defines% %compiler_flags_debug% ..\src\evolve.cc
::cl /nologo /MP %warnings% %warnings_release% %defines% %compiler_flags_release% ..\src\tiny_jpeg.cc

:: Windows XP build
::cl /nologo /MP %warnings% %warnings_release% %defines% %compiler_flags_release% ..\src\tiny_jpeg.cc /link /subsystem:windows,5.1
popd
