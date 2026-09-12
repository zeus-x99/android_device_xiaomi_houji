#!/usr/bin/env bash
set -euo pipefail
work=$(cd -- "$(dirname -- "$0")" && pwd)
tools=/home/zeus/lineage-houji/kernel-work/android14-6.1/prebuilts
export PATH="$tools/clang/host/linux-x86/clang-r487747c/bin:$tools/kernel-build-tools/linux-x86/bin:$PATH"
export LC_ALL=C TZ=UTC
mkdir -p "$work/logs/modules"
: > "$work/logs/modules/status.tsv"
started=${START_AT:-}

while IFS= read -r module; do
    if [[ -n "$started" && "$module" != "$started" ]]; then continue; fi
    started=""
    name=${module//\//_}
    echo "BUILD $module"
    if make -C "$work/kernel/sm8650-modules/$module" \
        "M=../sm8650-modules/$module" "KERNEL_SRC=$work/kernel/sm8650" \
        "OUT_DIR=$work/out/kernel" "O=$work/out/kernel" \
        ARCH=arm64 LLVM=1 LLVM_IAS=1 TARGET_BOARD_PLATFORM=pineapple \
        'CAMERA_COMPILE_TIME=2025-04-15 00:00:00 UTC' CAMERA_COMPILE_BY=builder CAMERA_COMPILE_HOST=houji \
        -j16 > "$work/logs/modules/$name.log" 2>&1; then
        printf '%s\tPASS\n' "$module" >> "$work/logs/modules/status.tsv"
    else
        code=$?
        printf '%s\tFAIL:%s\n' "$module" "$code" >> "$work/logs/modules/status.tsv"
        tail -n 45 "$work/logs/modules/$name.log"
        exit "$code"
    fi
done < "$work/modules.txt"
