#!/usr/bin/env bash
set -Eeuo pipefail

export EMSCRIPTEN="${EMSCRIPTEN:-/usr/lib/emscripten}"
export PATH="$EMSCRIPTEN:$PATH"
export EM_CACHE="${EM_CACHE:-/tmp/emscripten-cache}"

mkdir -p release
make -j"$(nproc)" \
  CC=emcc \
  ARCH=wasm32 \
  OSTYPE=emscripten \
  CFLAGS='-O0 -fno-strict-aliasing -Wall -pipe -g -MMD -fwrapv -fPIC -Wno-unused-result -std=gnu99 -DOSTYPE="emscripten" -DARCH="wasm32"' \
  LDFLAGS='-shared -sSIDE_MODULE=2 -sWASM_BIGINT=1 -sEXPORT_ALL=1 -Wl,--export=GetGameAPI' \
  release/game.so
mv release/game.so release/gamewasm32.so

