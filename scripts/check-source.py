#!/usr/bin/env python3
"""Reject the retired C/Kryon UI and ambiguous dependency imports."""
from pathlib import Path
import re

root = Path(__file__).resolve().parents[1]
local_modules = {p.stem for p in (root / "src").glob("*.zi")}
errors = []
for base in ("src", "tests"):
    for path in (root / base).rglob("*"):
        if not path.is_file():
            continue
        if path.suffix in {".c", ".h", ".kry"}:
            errors.append(f"{path.relative_to(root)}: maintained app code must be Ziran")
        if path.suffix != ".zi":
            continue
        for module in re.findall(r'#import\s+"([^"]+)"', path.read_text()):
            if "/" not in module and module not in local_modules:
                errors.append(f"{path.relative_to(root)}: qualify dependency {module}")
if errors:
    raise SystemExit("\n".join(errors))
print("Maintained source uses Ziran and explicit package imports")
