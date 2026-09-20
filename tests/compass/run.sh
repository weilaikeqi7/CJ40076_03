#!/bin/sh
# Run from any directory: sh /path/to/project/tests/compass/run.sh
# Optional: CC=/path/to/gcc sh tests/compass/run.sh
set -eu
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
root=$(CDPATH= cd -- "$here/../.." && pwd)
CC=${CC:-gcc}
command -v "$CC" >/dev/null 2>&1 || { printf 'FAIL: compiler not found: %s\n' "$CC" >&2; exit 1; }
build=$(mktemp -d "$here/.build.XXXXXX")
trap 'rm -rf "$build"' EXIT HUP INT TERM
status=0
for rate in 1000 100; do
    for model in 1 2 3; do
        printf 'Building COMPASS_MODEL=%s, tick rate=%s Hz\n' "$model" "$rate"
        "$CC" -std=c11 -Wall -Wextra -Werror -pedantic -O0 -g \
            "-DCOMPASS_MODEL=$model" "-DconfigTICK_RATE_HZ=$rate" \
            -I"$here/stubs" -I"$root/inc/device" -I"$root/inc/bsp" \
            "$root/src/device/dev_compass.c" "$here/test_compass.c" \
            -lm -o "$build/compass-$model.exe"
        for group in frames sequences mag power; do
            if ! "$build/compass-$model.exe" "$group"; then
                printf 'FAIL: model=%s tick_rate=%s group=%s\n' "$model" "$rate" "$group" >&2
                status=1
            fi
        done
    done
done
if test "$status" -eq 0; then
    printf 'PASS: all three production driver models at both tick rates\n'
else
    printf 'FAIL: one or more compass test groups failed\n' >&2
fi
exit "$status"
