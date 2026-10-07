#!/bin/sh
set -eu
ziran=${1:-./scripts/ziran.sh}
sqlite=$("$ziran" pkg path sqlite3)
mkdir -p build/sqlite
if [ ! -f build/sqlite/sqlite3.c ]; then
    (cd build/sqlite && "$sqlite/configure" --disable-shared > configure.log && make sqlite3.c > build.log)
fi
