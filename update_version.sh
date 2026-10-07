#!/bin/sh
# The newest numeric changelog entry is the release source of truth.
set -eu
cd "$(dirname "$0")"
python3 - <<'PY'
from pathlib import Path
import re
changelog = Path('CHANGELOG.md').read_text()
versions = re.findall(r'^## \[(\d+\.\d+\.\d+)\]', changelog, re.M)
if not versions:
    raise SystemExit('Add a numeric release entry to CHANGELOG.md first')
version = versions[0]
# Monotonic with the intended semantic version, independent of old entries.
major, minor, patch = map(int, version.split('.'))
code = major * 1000000 + minor * 1000 + patch
p = Path('droid/app/build.gradle')
s = re.sub(r'versionCode \d+', f'versionCode {code}', p.read_text())
s = re.sub(r'versionName "[^"]+"', f'versionName "{version}"', s)
p.write_text(s)
metadata = Path('packaging/linux/appimage/uku.appdata.xml')
date = re.search(r'^## \[.*?\] - (\d{4}-\d{2}-\d{2})', changelog, re.M).group(1)
xml = metadata.read_text()
xml = re.sub(r'<release version="[^"]+" date="[^"]+"/>', f'<release version="{version}" date="{date}"/>', xml, count=1)
metadata.write_text(xml)
print(f'Ukuvota version synchronized: {version}')
PY
