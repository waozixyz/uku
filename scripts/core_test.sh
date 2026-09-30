#!/bin/sh
# Ukuvota counting and ballots: native C and the portable Ziran bundle must
# agree.
set -eu
ziran=${1:-./scripts/ziran.sh}
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$root"
ziran_root=$("$ziran" pkg path ziran)
args="--root tests --module-path src --module-path $ziran_root/std"
for test in tally_test:TallyAnswer ballot_test:BallotAnswer; do
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
echo 'Ukuvota counting and ballots: native and portable tests pass'
