#!/usr/bin/env python3
"""Reject the retired C/Kryon UI and ambiguous dependency imports."""
from pathlib import Path
import fnmatch
import re
import subprocess

root = Path(__file__).resolve().parents[1]
local_modules = {p.stem for p in (root / "src").glob("*.zi")}
errors = []
tracked = subprocess.check_output(['git', 'ls-files'], cwd=root, text=True).splitlines()
for name in tracked:
    if name == '.gitmodules' or name.startswith('vendor/'):
        errors.append(f'{name}: dependencies must use the package lock')
for base in ("src", "tests"):
    for path in (root / base).rglob("*"):
        if not path.is_file():
            continue
        if path.suffix in {".c", ".h", ".kry"}:
            errors.append(f"{path.relative_to(root)}: maintained app code must be Ziran")
        if path.suffix != ".zi":
            continue
        if re.search(r'\b(?:ParseProcessDetail|BallotReplaceId|StorageMarkSynced|StoragePendingVotes|StoragePendingProposals|BrowserScriptInt|BrowserScript)\s*::', path.read_text()):
            errors.append(f"{path.relative_to(root)}: retired server or eval helper")
        for module in re.findall(r'#import\s+"([^"]+)"', path.read_text()):
            if "/" not in module and module not in local_modules:
                errors.append(f"{path.relative_to(root)}: qualify dependency {module}")
for name in ('SyncNetwork.java', 'ShareProvider.java', 'android_bridge.h'):
    if any((root / 'droid/app/src/main').rglob(name)):
        errors.append(f'{name}: retired Android bridge')
redirects = root / 'site/static/_redirects'
if redirects.exists():
    assets = ('index.js', 'index.wasm', 'browser-input.js', 'uku-logo.svg')
    for line in redirects.read_text().splitlines():
        rule = line.strip().split()
        if not rule or rule[0].startswith('#'):
            continue
        if any(fnmatch.fnmatchcase('/build/web/' + asset, rule[0]) for asset in assets):
            errors.append(f'site/static/_redirects: app assets must be served directly: {rule[0]}')
if errors:
    raise SystemExit("\n".join(errors))
print("Maintained source uses Ziran and explicit package imports")
