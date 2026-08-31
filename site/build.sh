#!/bin/sh

set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
root_dir=$(dirname -- "$script_dir")
out_dir="$root_dir/build/site"
web_dir="$root_dir/build/dist/web"

read_version() {
	awk '
		/versionName "/ {
			gsub(/^.*versionName "/, "")
			gsub(/".*$/, "")
			print
			found = 1
			exit
		}
		END {
			if (!found) exit 1
		}
	' "$root_dir/droid/app/build.gradle"
}

require_path() {
	if [ ! -e "$1" ]; then
		printf 'Error: required path missing: %s\n' "$1" >&2
		exit 1
	fi
}

copy_dir_contents() {
	src=$1
	dst=$2
	require_path "$src"
	mkdir -p "$dst"
	cp -R "$src"/. "$dst"/
}

copy_path() {
	src=$1
	dst=$2
	require_path "$src"
	mkdir -p "$(dirname -- "$dst")"
	cp "$src" "$dst"
}

expand_template_file() {
	src=$1
	dst=$2
	version=$3
	asset_version=$4
	mkdir -p "$(dirname -- "$dst")"
	sed \
		-e "s#\\\${version}#$version#g" \
		-e "s#\\\${asset_version}#$asset_version#g" \
		"$src" > "$dst"
}

copy_template_dir() {
	template_src_dir=$1
	template_dst_dir=$2
	version=$3
	asset_version=$4

	require_path "$template_src_dir"
	find "$template_src_dir" -type d -print | while IFS= read -r dir_path; do
		rel=${dir_path#"$template_src_dir"}
		mkdir -p "$template_dst_dir$rel"
	done
	find "$template_src_dir" -type f -print | while IFS= read -r file_path; do
		rel=${file_path#"$template_src_dir"/}
		case ${file_path##*.} in
			html|htm|txt|xml|json|css|js)
				expand_template_file "$file_path" "$template_dst_dir/$rel" "$version" "$asset_version"
				;;
			*)
				copy_path "$file_path" "$template_dst_dir/$rel"
				;;
		esac
	done
}

write_site_imports() {
	asset_version=$1
	{
		printf "@import url('/css/base.css?v=%s');\n" "$asset_version"
		printf "@import url('/css/components.css?v=%s');\n" "$asset_version"
		printf "@import url('/theme.css?v=%s');\n" "$asset_version"
	} > "$out_dir/style.css"
}

write_web_app_csp_html() {
	src=$1
	dst=$2
	cache_version=$(date +%s)
	csp="default-src 'self' data: blob:; connect-src 'self' https: wss:; script-src 'self' 'unsafe-eval' 'unsafe-inline'; style-src 'self' 'unsafe-inline'; worker-src 'self' 'unsafe-eval' 'unsafe-inline' data: blob:; img-src 'self' data: blob:; media-src 'self' data: blob:; object-src 'none'"
	meta="<meta http-equiv=\"Content-Security-Policy\" content=\"$csp\">"

	awk -v meta="$meta" -v version="$cache_version" '
		{
			gsub(/src="index\.js(\?v=[0-9A-Za-z._-]+)?"/, "src=\"index.js?v=" version "\"")
			if ($0 ~ /<meta http-equiv="Content-Security-Policy"/) {
				if (!done) print meta
				done = 1
				next
			}
			print
			if (!done && !inserted && $0 ~ /<meta charset="[^"]+"/) {
				print "    " meta
				inserted = 1
				done = 1
			}
			if (!done && $0 ~ /<\/head>/) {
				print "    " meta
				done = 1
			}
		}
	' "$src" > "$dst"
}

version=$(read_version)
asset_version=${SITE_ASSET_VERSION:-}
if [ -z "$asset_version" ]; then
	if git -C "$root_dir" diff --quiet --ignore-submodules HEAD -- 2>/dev/null; then
		asset_version=$(git -C "$root_dir" rev-parse --short HEAD 2>/dev/null || date +%s)
	else
		asset_version=$(date +%s)
	fi
fi

require_path "$web_dir/index.html"

rm -rf "$out_dir"
mkdir -p "$out_dir"

copy_dir_contents "$script_dir/css" "$out_dir/css"
copy_path "$script_dir/themes/uku.css" "$out_dir/theme.css"
write_site_imports "$asset_version"
copy_template_dir "$script_dir/static" "$out_dir" "$version" "$asset_version"
expand_template_file "$script_dir/index.html" "$out_dir/index.html" "$version" "$asset_version"

copy_dir_contents "$web_dir" "$out_dir/build/web"
copy_path "$root_dir/web-assets/uku-logo.svg" "$out_dir/web-assets/uku-logo.svg"
copy_path "$root_dir/web-assets/uku-logo.svg" "$out_dir/favicon.svg"

mkdir -p "$out_dir/screens"
copy_path "$script_dir/screens/dashboard-empty.png" "$out_dir/screens/dashboard-empty.png"
copy_path "$script_dir/screens/setup-process.png" "$out_dir/screens/setup-process.png"
copy_path "$script_dir/screens/schedule.png" "$out_dir/screens/schedule.png"
copy_path "$script_dir/screens/review.png" "$out_dir/screens/review.png"
copy_path "$script_dir/screens/voting.png" "$out_dir/screens/voting.png"
copy_path "$script_dir/screens/dashboard-active.png" "$out_dir/screens/dashboard-active.png"
copy_path "$script_dir/screens/og.png" "$out_dir/og.png"

write_web_app_csp_html "$out_dir/build/web/index.html" "$out_dir/build/web/index.html.tmp"
mv "$out_dir/build/web/index.html.tmp" "$out_dir/build/web/index.html"

printf 'built uku site -> %s\n' "$out_dir"
