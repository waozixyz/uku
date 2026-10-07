#!/bin/sh
# Compile the same Ziran app with Kryon's public Canvas host.
set -eu
ziran=${1:-./scripts/ziran.sh}
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$root"
compiler_root=$("$ziran" pkg path ziran)
kryon=$("$ziran" pkg path kryon)
sqlite=$("$ziran" pkg path sqlite3)
oqs=$("$ziran" pkg path liboqs)
emcc=${EMCC:-$HOME/emsdk/upstream/emscripten/emcc}
emcmake=${EMCMAKE:-$HOME/emsdk/upstream/emscripten/emcmake}
export EM_CACHE=${EM_CACHE:-$root/build/emscripten-cache}
rm -rf build/dist/web
mkdir -p build/web/c build/sqlite build/dist/web build/web/oqs
sh scripts/prepare-sqlite.sh "$ziran"
if [ ! -f build/web/oqs/lib/liboqs.a ]; then
    "$emcmake" cmake -S "$oqs" -B build/web/oqs \
        -DCMAKE_BUILD_TYPE=MinSizeRel -DBUILD_SHARED_LIBS=OFF \
        -DOQS_BUILD_ONLY_LIB=ON -DOQS_USE_OPENSSL=OFF -DOQS_DIST_BUILD=OFF \
        -DOQS_USE_PTHREADS=OFF -DOQS_OPT_TARGET=generic \
        -DOQS_MINIMAL_BUILD=SIG_ml_dsa_44 > build/web/oqs-configure.log
    cmake --build build/web/oqs --target oqs -j2 > build/web/oqs-build.log
fi
rm -rf build/web/c
mkdir -p build/web/c
"$ziran" build --project --define PLATFORM_WEB --target=c \
    -o build/web/c "$kryon/src/backend/canvas_run.zi" "$kryon/src/backend/canvas_raster.zi"
"$emcc" -O2 -I"$compiler_root/include" -Ibuild/web/c \
    build/web/c/*.c build/sqlite/sqlite3.c build/web/oqs/lib/liboqs.a \
    -DSQLITE_THREADSAFE=0 -DSQLITE_OMIT_LOAD_EXTENSION -DSQLITE_DEFAULT_MEMSTATUS=0 \
    --js-library "$compiler_root/web/ziran_web.js" \
    --embed-file "$kryon/assets/fonts/LiberationSans-Regular.ttf@/kryon-font.ttf" \
    -sEXPORTED_RUNTIME_METHODS=FS -sFORCE_FILESYSTEM=1 -lidbfs.js \
    -sASYNCIFY -sALLOW_MEMORY_GROWTH=1 -sSTACK_SIZE=16777216 \
    -sENVIRONMENT=web --shell-file src/web_shell.html \
    -o build/dist/web/index.html
cp web-assets/uku-logo.svg build/dist/web/
python3 scripts/package-web.py
