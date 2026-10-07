#!/usr/bin/env python3
"""Package the current browser build for web and itch distribution."""
from pathlib import Path
import zipfile

root = Path(__file__).resolve().parents[1]
web = root / 'build/dist/web'
if not all((web / name).is_file() for name in ('index.html', 'index.js', 'index.wasm')):
    raise SystemExit('Build the web profile first: make web')
for name in ('uku-web.zip', 'uku-itch-html5.zip'):
    with zipfile.ZipFile(root / 'build/dist' / name, 'w', zipfile.ZIP_DEFLATED) as archive:
        for path in sorted(web.rglob('*')):
            if path.is_file():
                archive.write(path, path.relative_to(web))
