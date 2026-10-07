#!/bin/sh
# Package the current native Ziran app, without a second runtime or UI build.
set -eu
umask 022
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$root"
version=$(sed -n 's/^## \[\([0-9][0-9.]*\)\].*/\1/p' CHANGELOG.md | head -n 1)
stage=build/package-linux
rm -rf "$stage"
mkdir -p "$stage/usr/bin" "$stage/usr/share/applications" "$stage/usr/share/icons/hicolor/256x256/apps" "$stage/usr/share/metainfo"
cp build/uku-desktop "$stage/usr/bin/uku"
cp packaging/linux/appimage/uku.desktop "$stage/usr/share/applications/xyz.waozi.uku.desktop"
cp packaging/linux/appimage/uku.png "$stage/usr/share/icons/hicolor/256x256/apps/xyz.waozi.uku.png"
cp packaging/linux/appimage/uku.appdata.xml "$stage/usr/share/metainfo/xyz.waozi.uku.metainfo.xml"
case ${1:-} in
    deb)
        arch=$(dpkg --print-architecture)
        mkdir -p "$stage/DEBIAN" build/dist/deb
        cat > "$stage/DEBIAN/control" <<EOF
Package: uku
Version: $version
Architecture: $arch
Maintainer: Waozi <waozi@proton.me>
Depends: libsdl2-2.0-0, libcairo2, libsqlite3-0
Description: Collective decisions with visible resistance
 Local consent voting with proposal collection, weighted resistance and results.
EOF
        dpkg-deb --root-owner-group --build "$stage" "build/dist/deb/uku_${version}_${arch}.deb"
        ;;
    appimage)
        arch=$(uname -m)
        mkdir -p build/dist/linux
        export APPIMAGE_EXTRACT_AND_RUN=1 ARCH=$arch
        linuxdeploy --appdir "$stage" --executable "$stage/usr/bin/uku" \
            --desktop-file "$stage/usr/share/applications/xyz.waozi.uku.desktop" \
            --icon-file "$stage/usr/share/icons/hicolor/256x256/apps/xyz.waozi.uku.png"
        appimagetool "$stage" "build/dist/linux/uku-linux-${arch}.AppImage"
        ;;
    *) printf 'Usage: %s deb|appimage\n' "$0" >&2; exit 2 ;;
esac
