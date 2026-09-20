#!/bin/sh
set -eu
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
root=$(CDPATH= cd -- "$here/../.." && pwd)
build=$(mktemp -d)
trap 'rm -rf "$build"' EXIT HUP INT TERM
for opt in 0 2; do
    "${CC:-gcc}" -std=c11 -Wall -Wextra -Werror -O"$opt" \
        -I"$root/inc/app" -I"$root/inc/device" \
        "$root/src/app/app_key.c" "$here/test_key.c" -o "$build/key.exe"
    "$build/key.exe"
done
