#!/bin/sh
# Generate the app once, then compile that source for each requested ABI.
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$root"
ziran=${ZIRAN:-./scripts/ziran.sh}
kryon=$("$ziran" pkg path kryon)
sh scripts/prepare-sqlite.sh "$ziran"
rm -rf build/android/c
"$ziran" build --project --root src/hosts --define ANDROID_BUILD --define PLATFORM_ANDROID \
    --target=c --entry android:main -o build/android/c src/hosts/android.zi
mkdir -p droid/app/src/main/assets
cp "$kryon/assets/fonts/LiberationSans-Regular.ttf" droid/app/src/main/assets/
exec ./droid/gradlew -p droid --no-daemon "${ANDROID_TASK:-assembleDebug}" \
    -Puku.generated="$root/build/android/c" \
    -Puku.raylib="$("$ziran" pkg path raylib)" \
    -Puku.oqs="$("$ziran" pkg path liboqs)" \
    -Puku.sqlite="$root/build/sqlite" \
    -Puku.ziran="$("$ziran" pkg path ziran)" "$@"
