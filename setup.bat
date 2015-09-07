@echo off

echo "stb..."
if NOT EXIST third_party\stb goto stb_clone
goto stb_pull
:stb_clone
git clone https://github.com/nothings/stb.git third_party/stb
goto stb_end
:stb_pull
pushd third_party\stb
git pull
popd
:stb_end

echo "tiny_jpeg..."
IF NOT EXIST src\tiny_jpeg goto tje_clone
goto tje_pull
:tje_clone
git clone https://github.com/serge-rgb/TinyJPEG.git src/tiny_jpeg
goto tje_end

:tje_pull
pushd src\tiny_jpeg
git pull
popd

:tje_end

echo "libserg"

IF NOT EXIST src\libserg goto libserg_clone
goto libserg_pull
:libserg_clone
git clone https://github.com/serge-rgb/libserg.git src/libserg
goto libserg_end
:libserg_pull
pushd src\libserg
git pull
popd
:libserg_end
