#!/bin/sh
# Builds liboqs with only ML-DSA-44 from the locked source; Uku's account keys
# sign through it.
set -eu
ziran=${1:-./scripts/ziran.sh}
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$root"

liboqs_build=build/liboqs
if [ ! -f "$liboqs_build/lib/liboqs.a" ]; then
    cmake -S "$("$ziran" pkg path liboqs)" -B "$liboqs_build" \
        -DCMAKE_BUILD_TYPE=MinSizeRel -DBUILD_SHARED_LIBS=OFF \
        -DOQS_BUILD_ONLY_LIB=ON -DOQS_USE_OPENSSL=OFF -DOQS_DIST_BUILD=OFF \
        -DOQS_OPT_TARGET=generic -DOQS_MINIMAL_BUILD=SIG_ml_dsa_44 > /dev/null
    cmake --build "$liboqs_build" --target oqs > /dev/null
fi
