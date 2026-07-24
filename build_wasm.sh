#!/bin/bash
source /home/emrys/emsdk/emsdk_env.sh
emcc src/*.c -o /home/emrys/moon-web/src/moon.js \
    -s MODULARIZE=1 \
    -s EXPORT_NAME="MoonModule" \
    -s EXPORTED_RUNTIME_METHODS='["cwrap", "ccall"]' \
    -s ALLOW_MEMORY_GROWTH=1 \
    -s ASYNCIFY=1 \
    -s EXPORTED_FUNCTIONS='["_executeMoonCode", "_initMoonWeb", "_setCompilerFlags", "_malloc", "_free"]' \
    -s EXPORT_ES6=1 \
    -O3 -I./src
