#!/usr/bin/env bash
# 主机回归统一入口：任一套件失败均返回非零；无需连接硬件。
set -uo pipefail
here="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
status=0
for suite in adc app compass display gnss key measurement runtime; do
    printf '\n=== %s ===\n' "$suite"
    if ! bash "$here/$suite/run.sh"; then
        status=1
    fi
done
exit "$status"
