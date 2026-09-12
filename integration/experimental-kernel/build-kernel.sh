#!/usr/bin/env bash
set -euo pipefail
work=$(cd -- "$(dirname -- "$0")" && pwd)
tools=/home/zeus/lineage-houji/kernel-work/android14-6.1/prebuilts
export PATH="$tools/clang/host/linux-x86/clang-r487747c/bin:$tools/kernel-build-tools/linux-x86/bin:$PATH"
export LC_ALL=C TZ=UTC
export SOURCE_DATE_EPOCH=$(git -C "$work/kernel/sm8650" show -s --format=%ct HEAD)
export KBUILD_BUILD_TIMESTAMP=$(date -u -d "@$SOURCE_DATE_EPOCH" '+%a %b %d %T UTC %Y')
export KBUILD_BUILD_USER=builder KBUILD_BUILD_HOST=houji KBUILD_BUILD_VERSION=1
mkdir -p "$work/out/kernel"
cd "$work"
bash kernel/sm8650/scripts/kconfig/merge_config.sh -m -O out/kernel \
 kernel/sm8650/arch/arm64/configs/gki_defconfig \
 kernel/sm8650/arch/arm64/configs/vendor/pineapple_GKI.config \
 kernel/sm8650/arch/arm64/configs/vendor/houji_GKI.config
make -C kernel/sm8650 "O=$work/out/kernel" ARCH=arm64 LLVM=1 LLVM_IAS=1 olddefconfig
make -C kernel/sm8650 "O=$work/out/kernel" ARCH=arm64 LLVM=1 LLVM_IAS=1 -j16 Image modules
