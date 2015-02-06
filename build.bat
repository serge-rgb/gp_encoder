@echo off

set warnings=/WX /W4 /wd4100 /wd4310 /wd4189
:: warning 4189 is useful for polisihing code, but it's a pain while developing.
set warnings_release=%warnings% /wd4189

set compiler_flags=/D_CRT_SECURE_NO_WARNINGS /FC /Zi /I../src
set compiler_flags_debug=%compiler_flags% %warnings% /DTJE_DEBUG /O2 /Oi /fp:fast
set compiler_flags_release=%compiler_flags% %warnings_release% /O2
:: set linker_flags= /opt:ref

IF NOT EXIST build mkdir build
pushd build
cl /nologo /MP %warnings% %defines% %compiler_flags_debug% ..\src\win_evolve.cc
::cl /nologo /MP %warnings% %defines% %compiler_flags_release% ..\src\evolve.cc
popd
