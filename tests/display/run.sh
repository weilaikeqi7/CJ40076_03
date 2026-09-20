#!/usr/bin/env bash
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
build="$(mktemp -d)"
trap 'rm -rf -- "$build"' EXIT
for opt in 0 2; do
    "${CC:-gcc}" -std=c11 -Wall -Wextra -Werror -O"$opt" \
        -I"$root/inc/app" -I"$root/inc/device" \
        "$root/tests/display/test_display.c" "$root/src/app/app_display.c" \
        -lm -o "$build/display$opt.exe"
    "$build/display$opt.exe"
done
