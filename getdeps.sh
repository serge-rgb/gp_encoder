if [ ! -d third_party/stb ]; then
    git clone https://github.com/nothings/stb.git third_party/stb
else
    cd third_party/stb
    git pull
    cd ../..
fi

if [ ! -d third_party/libnuwen ]; then
    git clone https://github.com/serge-rgb/libnuwen third_party/libnuwen
else
    cd third_party/libnuwen
    git pull
    cd ../..
fi
