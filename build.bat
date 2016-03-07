@echo off

REM 4204 Non-constant aggregate initializer (it's valid C99)
REM 4100 Unreferenced func param (cleanup)
REM 4820 struct padding
REM 4255 () != (void)
REM 4668 Macro not defined. Subst w/0
REM 4710 Func not inlined
REM 4711 Auto inline
REM 4189 Init. Not ref
REM 4464 Relative path with ..
set comment_for_cleanup=/wd4100 /wd4189
set suppressed=%comment_for_cleanup% /wd4820 /wd4255 /wd4668 /wd4710 /wd4711 /wd4204 /wd4464

::set defines=-D_CRT_SECURE_NO_WARNINGS -DNDEBUG
set defines=-D_CRT_SECURE_NO_WARNINGS
set includes=/I ../src /I ../src/tiny_jpeg /I ../src/libserg /I ../third_party

IF NOT EXIST build mkdir build
pushd build
cl /nologo /FC /Wall /wd4464 /WX /Zi /Od /fp:fast %includes% %defines% %suppressed% ..\src\jpeg_test.c OpenCL.lib
::cl /nologo /FC /Wall /WX /Zi /Ox %includes% %defines% %suppressed% ..\src\main.c ..\src\extended_jpeg.c OpenCL.lib /Fe:JpegEvolve.exe
popd
