@echo off

REM 4100 Unreferenced func param (cleanup)
REM 4820 struct padding
REM 4255 () != (void)
REM 4668 Macro not defined. Subst w/0
REM 4710 Func not inlined
REM 4711 Auto inline
REM 4189 Init. Not ref
set comment_for_cleanup=/wd4100 /wd4189
set suppressed=%comment_for_cleanup% /wd4820 /wd4255 /wd4668 /wd4710 /wd4711

set defines=-D_CRT_SECURE_NO_WARNINGS -DTJE_DEBUG
set includes=/I ../src /I ../src/tiny_jpeg /I ../src/libserg

IF NOT EXIST build mkdir build
pushd build
cl /nologo /Wall /WX /Zi /Od /fp:fast %includes% %defines% %suppressed% ..\src\jpeg_test.c
popd
