if [ ! -d third_party/stb ]; then
    git clone https://github.com/nothings/stb.git third_party/stb
else
    cd third_party/stb
    git pull
    cd ../..
fi

if [ ! -d src/tiny_jpeg ]; then
    git clone https://github.com/serge-rgb/TinyJPEG.git src/tiny_jpeg
else
    cd src/tiny_jpeg
    git pull
    cd ../..
fi
