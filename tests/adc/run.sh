#!/usr/bin/env bash
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
build="$(mktemp -d)"
trap 'rm -rf -- "$build"' EXIT
for opt in 0 2; do
    "${CC:-gcc}" -std=c11 -Wall -Wextra -Werror -O"$opt" \
        -I"$root/tests/adc/stubs" -I"$root/inc/bsp" -I"$root/inc/app" \
        -I"$root/inc/device" -I"$root/tests/app/stubs" \
        "$root/tests/adc/test_adc.c" "$root/src/bsp/bsp_adc.c" \
        "$root/src/app/app_thermal.c" -lm -o "$build/adc$opt.exe"
    "$build/adc$opt.exe"
done
