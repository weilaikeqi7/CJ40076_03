#!/usr/bin/env bash
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
build="$(mktemp -d)"
trap 'rm -rf -- "$build"' EXIT
cc="${CC:-gcc}"
for model in 1 2 3; do
    "$cc" -std=c11 -Wall -Wextra -Werror -O2 -DCOMPASS_MODEL="$model" \
        -I"$root/tests/measurement/stubs" -I"$root/tests/app/stubs" \
        -I"$root/inc/app" -I"$root/inc/device" -I"$root/inc/bsp" \
        "$root/tests/measurement/test_measurement.c" \
        "$root/src/app/app_measure.c" "$root/src/device/dev_ranger.c" \
        -o "$build/measurement_${model}.exe"
    "$build/measurement_${model}.exe"
done
