#!/bin/sh
# Ukuvota counting: native C and the portable Ziran bundle must agree.
set -eu
ziran=${1:-./scripts/ziran.sh}
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$root"
ziran_root=$("$ziran" pkg path ziran)
args="--root tests --module-path src --module-path $ziran_root/std"
work=build/tally-test
rm -rf "$work"
mkdir -p "$work"
"$ziran" build $args --target=c -o "$work/c" tests/tally_test.zi
${CC:-cc} -std=c99 -O2 -I"$ziran_root/include" -I"$work/c" "$work"/c/*.c -o "$work/native"
env -u DISPLAY -u WAYLAND_DISPLAY "$work/native"
"$ziran" bundle $args --entry tally_test:TallyAnswer -o "$work/portable.zib" tests/tally_test.zi
test "$("$ziran" run "$work/portable.zib")" = 42
echo 'Ukuvota counting: native and portable tests pass'
