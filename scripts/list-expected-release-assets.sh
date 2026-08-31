#!/bin/sh

set -eu

if [ $# -gt 0 ]; then
	version=$1
else
	script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
	root_dir=$(dirname -- "$script_dir")
	version=$(awk '
		/versionName "/ {
			gsub(/^.*versionName "/, "")
			gsub(/".*$/, "")
			print
			exit
		}
	' "$root_dir/droid/app/build.gradle")
fi

if [ -z "$version" ]; then
	printf 'Error: version is empty\n' >&2
	exit 1
fi

cat <<EOF
uku-$version.apk
uku-$version.aab
uku-web.zip
uku-linux-x86_64.AppImage
uku-linux-aarch64.AppImage
uku_${version}_amd64.deb
uku_${version}_arm64.deb
EOF
