#!/bin/sh
# Ukuvota counting and ballots: native C and the portable Ziran bundle must
# agree.
set -eu
ziran=${1:-./scripts/ziran.sh}
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$root"
ziran_root=$("$ziran" pkg path ziran)
args="--root tests --module-path src --module-path $ziran_root/std"
for test in tally_test:TallyAnswer ballot_test:BallotAnswer process_test:ProcessAnswer; do
    module=${test%%:*}
    work=build/core-test/$module
    rm -rf "$work"
    mkdir -p "$work"
    "$ziran" build $args --target=c -o "$work/c" "tests/$module.zi"
    ${CC:-cc} -std=c99 -O2 -I"$ziran_root/include" -I"$work/c" "$work"/c/*.c -o "$work/native"
    env -u DISPLAY -u WAYLAND_DISPLAY "$work/native"
    "$ziran" bundle $args --entry "$test" -o "$work/portable.zib" "tests/$module.zi"
    test "$("$ziran" run "$work/portable.zib")" = 42
done
# Storage calls into SQLite's C library, so it runs natively only.
work=build/core-test/storage_test
rm -rf "$work" build/core-test/storage.sqlite3
mkdir -p "$work"
"$ziran" build --project --target=c --entry storage_test:main -o "$work/c" tests/storage_test.zi
# Account keys sign through liboqs, built once from the locked source with
# only ML-DSA-44.
liboqs_build=build/liboqs
if [ ! -f "$liboqs_build/lib/liboqs.a" ]; then
    cmake -S "$("$ziran" pkg path liboqs)" -B "$liboqs_build" \
        -DCMAKE_BUILD_TYPE=MinSizeRel -DBUILD_SHARED_LIBS=OFF \
        -DOQS_BUILD_ONLY_LIB=ON -DOQS_USE_OPENSSL=OFF -DOQS_DIST_BUILD=OFF \
        -DOQS_OPT_TARGET=generic -DOQS_MINIMAL_BUILD=SIG_ml_dsa_44 > /dev/null
    cmake --build "$liboqs_build" --target oqs > /dev/null
fi
${CC:-cc} -std=c99 -O2 -I"$ziran_root/include" -I"$work/c" "$work"/c/*.c "$liboqs_build/lib/liboqs.a" ${SQLITE_LDLIBS:--lsqlite3} -o "$work/native"
env -u DISPLAY -u WAYLAND_DISPLAY "$work/native"
echo 'Ukuvota counting, ballots, processes, and storage: tests pass'
