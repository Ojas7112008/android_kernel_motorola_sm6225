#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0
set -euo pipefail
ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)
CLANG_DIR=${CLANG_DIR:-"$ROOT/../.toolchains/android-16-r547379"}
OUT_DIR=${OUT_DIR:-"$ROOT/out"}
ARTIFACT_DIR=${ARTIFACT_DIR:-"$ROOT/artifacts"}
JOBS=${JOBS:-4}
cd "$ROOT"
mkdir -p "$OUT_DIR" "$ARTIFACT_DIR"
python3 - "$ROOT" "$CLANG_DIR" <<'VERIFY'
import json,os,re,subprocess,sys
from pathlib import Path
root,clang=map(Path,sys.argv[1:]);pins=json.loads((root/'scripts/devon/source-pins.json').read_text())
if subprocess.run(['git','-C',str(root),'merge-base','--is-ancestor',pins['kernel_commit'],'HEAD']).returncode:
    raise SystemExit('Pinned LineageOS base is not an ancestor of this checkout')
selected=os.environ.get('BAKASU_COMMIT',pins['bakasu_commit'])
if not re.fullmatch(r'[0-9a-f]{40}',selected): raise SystemExit('BakaSU commit must be a full SHA')
for repo,pin in [(root/'KernelSU',selected)]:
    actual=subprocess.check_output(['git','-C',str(repo),'rev-parse','HEAD']).decode().strip()
    if actual!=pin: raise SystemExit(f'Source pin mismatch: {repo}: {actual}')
    if subprocess.run(['git','-C',str(repo),'merge-base','--is-ancestor',pins['bakasu_commit'],actual]).returncode:
        raise SystemExit('Selected BakaSU must descend from the validated baseline')
    if subprocess.check_output(['git','-C',str(repo),'rev-parse','--is-shallow-repository']).strip()!=b'false':
        raise SystemExit('Full BakaSU history is required for its native version calculation')
if subprocess.check_output(['git','-C',str(root/'KernelSU'),'status','--porcelain']):
    raise SystemExit('BakaSU source must remain clean')
if not (clang/'bin/clang').is_file(): raise SystemExit('Missing pinned Android Clang; see docs/BAKASU_BUILD.md')
VERIFY
export PATH="$CLANG_DIR/bin:$PATH"
export LD_LIBRARY_PATH="$CLANG_DIR/lib64:${LD_LIBRARY_PATH:-}"
export ARCH=arm64 LLVM=1 CLANG_TRIPLE=aarch64-linux-gnu-
export CROSS_COMPILE=aarch64-linux-gnu- CROSS_COMPILE_ARM32=arm-linux-gnueabi-
export KBUILD_BUILD_USER=devon KBUILD_BUILD_HOST=local KBUILD_BUILD_VERSION=1
export KBUILD_BUILD_TIMESTAMP
KBUILD_BUILD_TIMESTAMP=$(git show -s --format=%cD HEAD)
clang --version | tee "$ARTIFACT_DIR/compiler.txt"
if ! grep -q 'r547379' "$ARTIFACT_DIR/compiler.txt"; then
    echo 'Expected Android Clang r547379' >&2
    exit 1
fi
if [[ ${1:-} != --build-only ]]; then
    make O="$OUT_DIR" ARCH=arm64 LLVM=1 vendor/bengal-perf_defconfig 2>&1 | tee "$ARTIFACT_DIR/configure.log"
    scripts/kconfig/merge_config.sh -m -O "$OUT_DIR" "$OUT_DIR/.config" \
        arch/arm64/configs/vendor/ext_config/moto-bengal.config \
        arch/arm64/configs/vendor/ext_config/devon-default.config \
        arch/arm64/configs/vendor/ext_config/devon-bakasu.config 2>&1 | tee "$ARTIFACT_DIR/merge-config.log"
    make O="$OUT_DIR" ARCH=arm64 LLVM=1 olddefconfig 2>&1 | tee "$ARTIFACT_DIR/olddefconfig.log"
fi
for symbol in KSU KSU_MANUAL_HOOK KSU_MANUAL_HOOK_AUTO_SETUID_HOOK KSU_MANUAL_HOOK_AUTO_INITRC_HOOK KSU_MANUAL_HOOK_AUTO_INPUT_HOOK KALLSYMS_ALL LTO_CLANG CFI_CLANG; do
    grep -qx "CONFIG_${symbol}=y" "$OUT_DIR/.config"
done
if grep -Eq '^CONFIG_KSU_(SUSFS|TRACEPOINT_HOOK)=y' "$OUT_DIR/.config"; then
    echo 'Unexpected hook configuration' >&2
    exit 1
fi
cp "$OUT_DIR/.config" "$ARTIFACT_DIR/kernel.config"
[[ ${1:-} == --configure-only ]] && exit 0
make -j"$JOBS" O="$OUT_DIR" ARCH=arm64 LLVM=1 Image dtbs modules 2>&1 | tee "$ARTIFACT_DIR/build.log"
python3 scripts/devon/collect-artifacts.py "$OUT_DIR" "$ARTIFACT_DIR"
