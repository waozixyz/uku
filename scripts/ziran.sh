#!/bin/sh
# Use the canonical local compiler when explicitly overridden, otherwise the
# installed toolchain. Dependencies are resolved by Ziran, never vendored here.
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$root"
compiler=ziran
if [ -f ziran.local.toml ]; then
    local_root=$(python3 -c 'import tomllib; print(tomllib.load(open("ziran.local.toml", "rb")).get("overrides", {}).get("ziran", ""))')
    if [ -n "$local_root" ]; then
        compiler="$local_root/build/bin/ziran"
    fi
fi
if [ "${ZIRAN_LOCKED:-0}" = 1 ] || [ ! -f ziran.local.toml ]; then
    case ${1:-} in
        build|check|ir|bundle|run|install)
            verb=$1
            shift
            exec "$compiler" "$verb" --locked "$@"
            ;;
        pkg) exec "$compiler" "$@" --locked ;;
    esac
fi
exec "$compiler" "$@"
