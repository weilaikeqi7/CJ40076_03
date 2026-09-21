#!/usr/bin/env bash
set -euo pipefail

root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
build="$(mktemp -d)"
trap 'rm -rf -- "$build"' EXIT
cc="${CC:-gcc}"

"$cc" -std=c11 -Wall -Wextra -Werror -pedantic -D_DEFAULT_SOURCE -DM_PI=3.14159265358979323846 -O0 -g \
    -I"$root/tests/gnss/stubs" -I"$root/inc/common" -I"$root/inc/app" -I"$root/inc/device" -I"$root/inc/bsp" \
    "$root/src/device/dev_gnss.c" "$root/src/app/app_coord.c" "$root/tests/gnss/test_gnss.c" \
    -lm -o "$build/test_gnss.exe"
"$build/test_gnss.exe"
