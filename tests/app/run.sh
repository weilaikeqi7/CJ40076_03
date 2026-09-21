#!/usr/bin/env bash
set -euo pipefail

root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
build="$(mktemp -d)"
trap 'rm -rf -- "$build"' EXIT
cc="${CC:-gcc}"

for model in 1 2 3; do
    for suite in calib store; do
        "$cc" -std=c11 -Wall -Wextra -Werror -O2 \
            -DCOMPASS_MODEL="$model" \
            -I"$root/tests/app/stubs" -I"$root/inc/common" -I"$root/inc/app" -I"$root/inc/device" \
            "$root/tests/app/test_${suite}.c" "$root/src/app/app_${suite}.c" \
            -o "$build/${suite}_${model}.exe"
        "$build/${suite}_${model}.exe"
    done
    "$cc" -std=c11 -Wall -Wextra -Werror -O2 \
        -DCOMPASS_MODEL="$model" \
        -I"$root/tests/app/stubs" -I"$root/inc/common" -I"$root/inc/app" -I"$root/inc/device" -I"$root/inc/bsp" \
        "$root/tests/app/test_shutdown.c" "$root/src/app/app_power.c" \
        -o "$build/shutdown_${model}.exe"
    "$build/shutdown_${model}.exe"
done
