#!/usr/bin/env bash
set -euo pipefail

root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
build="$(mktemp -d)"
trap 'rm -rf -- "$build"' EXIT
cc="${CC:-gcc}"

for opt in -O0 -O2; do
    "$cc" -std=c11 -Wall -Wextra -Werror "$opt" \
        -I"$root/tests/runtime/stubs" -I"$root/inc" -I"$root/inc/app" \
        "$root/tests/runtime/test_main.c" -o "$build/test_main_${opt#-}.exe"
    "$build/test_main_${opt#-}.exe"

    "$cc" -std=c11 -Wall -Wextra -Werror "$opt" -ffunction-sections -fdata-sections \
        -Wl,--gc-sections \
        -I"$root/tests/app/stubs" -I"$root/inc" -I"$root/inc/app" -I"$root/inc/device" -I"$root/inc/bsp" \
        "$root/tests/runtime/test_heartbeat.c" "$root/tests/runtime/app_stubs.c" \
        -o "$build/test_heartbeat_${opt#-}.exe"
    "$build/test_heartbeat_${opt#-}.exe"
done
