echo "stb..."
if [ ! -d third_party/stb ]; then
    git clone https://github.com/nothings/stb.git third_party/stb
else
    cd third_party/stb
    git pull
    cd ../..
fi

echo "tiny_jpeg..."
if [ ! -d src/tiny_jpeg ]; then
    git clone https://github.com/serge-rgb/TinyJPEG.git src/tiny_jpeg
else
    cd src/tiny_jpeg
    git pull
    cd ../..
fi

echo "libserg..."
if [ ! -d src/libserg ]; then
    git clone https://github.com/serge-rgb/libserg.git src/libserg
else
    cd src/libserg
    git pull
    cd ../..
fi
