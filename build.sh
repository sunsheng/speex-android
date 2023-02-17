#!/bin/env bash

export CC=$NDK/toolchains/llvm/prebuilt/darwin-x86_64/bin/aarch64-linux-android21-clang
export CFLAGS='-O3'    
./autogen.sh
./configure --prefix=`pwd`/dist  --host=aarch64-linux-android --enable-shared --enable-static
make clean; make; make install