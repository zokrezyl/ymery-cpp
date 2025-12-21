#!/bin/bash
cd "$(dirname "$0")"

export PATH="/usr/local/bin:/usr/bin:/bin"

if [ ! -f "./build/ymery-cli" ]; then
    echo "Error: ymery-cli not found. Build first with:"
    echo "  export PATH=\"/usr/local/bin:/usr/bin:/bin\""
    echo "  cmake -B build -DCMAKE_C_COMPILER=/usr/bin/gcc -DCMAKE_CXX_COMPILER=/usr/bin/g++ && make -C build"
    exit 1
fi

./build/ymery-cli -l ./layout -m app "$@"
