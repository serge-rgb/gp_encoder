@echo off

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

IF NOT EXIST src/tiny_jpeg goto tje_clone
goto tje_pull
:tje_clone
git clone https://github.com/serge-rgb/TinyJPEG.git src/tiny_jpeg
goto tje_end

:tje_pull
pushd src\tiny_jpeg
git pull
popd
goto tje_end

:tje_end
