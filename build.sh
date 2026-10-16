#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0
# Build the non-SUSFS Devon kernel and replace only the supplied Axion boot kernel.
set -euo pipefail
ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
cd "$ROOT"
BASE_BOOT=${BASE_BOOT:-'/mnt/d/Phone expriment/Moto G32/Axion/Last Backup/boot.img'}
JOBS=${JOBS:-4}
CLANG_DIR=${CLANG_DIR:-"$ROOT/../.toolchains/android-16-r547379"}
SELECTION=latest
CHECK_ONLY=0
REPACK_ONLY=0
KEEP_OUT=0
KERNEL_ARTIFACTS="$ROOT/artifacts"
usage() {
    cat <<'USAGE'
Usage: ./build.sh [options]
  --base-boot PATH       Axion base boot.img (the provided D: image is the default)
  --jobs N               Build parallelism (default: JOBS=4)
  --pinned               Use the previously validated BakaSU commit, without updating
  --check                Check inputs and report selected upstream commit; do not build
  --repack-only          Reuse a successful kernel build instead of recompiling
  --kernel-artifacts DIR Saved build directory for --repack-only (default: artifacts/)
  --keep-out             Keep intermediate files for incremental builds (about 4 GiB)
  -h, --help            Show this help

Default: fetch BakaSU main, build Image/dtbs/modules, create a new boot.img under
artifacts/boot-builds/, then remove out/ after success. Never flashes a device.
USAGE
}
die() { printf 'ERROR: %s\n' "$*" >&2; exit 1; }
while (($#)); do
    case "$1" in
        --base-boot|--jobs|--kernel-artifacts)
            (($# >= 2)) || die "Missing value for $1"
            case "$1" in
                --base-boot) BASE_BOOT=$2 ;;
                --jobs) JOBS=$2 ;;
                --kernel-artifacts) KERNEL_ARTIFACTS=$2 ;;
            esac
            shift 2 ;;
        --pinned) SELECTION=pinned; shift ;;
        --check) CHECK_ONLY=1; shift ;;
        --repack-only) REPACK_ONLY=1; shift ;;
        --keep-out) KEEP_OUT=1; shift ;;
        -h|--help) usage; exit 0 ;;
        *) die "Unknown argument: $1 (see --help)" ;;
    esac
done
[[ $JOBS =~ ^[1-9][0-9]*$ ]] || die 'JOBS must be a positive integer'
for command in python3 git realpath flock; do command -v "$command" >/dev/null || die "Missing $command"; done
if [[ $BASE_BOOT =~ ^[A-Za-z]:[\\/] ]]; then BASE_BOOT=$(wslpath -u "$BASE_BOOT"); fi
BASE_BOOT=$(realpath -e -- "$BASE_BOOT")
[[ -f $BASE_BOOT ]] || die 'Base boot image is missing'
python3 scripts/devon/repack_boot.py --base "$BASE_BOOT"
[[ -d KernelSU/kernel ]] || git submodule update --init -- KernelSU
[[ -z $(git -C KernelSU status --porcelain) ]] || die 'BakaSU has local changes; refusing to overwrite them'
BASELINE=$(python3 -c 'import json; print(json.load(open("scripts/devon/source-pins.json"))["bakasu_commit"])')
BAKASU_URL=https://github.com/Baka-SU/BakaSU.git
if ((CHECK_ONLY)); then
    if [[ $SELECTION == latest ]]; then
        SELECTED=$(git ls-remote --exit-code "$BAKASU_URL" refs/heads/main | cut -f1)
    else
        SELECTED=$BASELINE
    fi
    [[ -x $CLANG_DIR/bin/clang ]] || die 'Pinned Android Clang is missing; see docs/BAKASU_BUILD.md'
    printf 'Preflight passed. BakaSU %s: %s\n' "$SELECTION" "$SELECTED"
    exit 0
fi
# Serialize builds/repacking without putting a lock inside the removable out/ tree.
exec 9>"$(git rev-parse --git-path devon-build.lock)"
flock -n 9 || die 'Another build.sh run is active'
if [[ $SELECTION == latest ]]; then
    FETCH_ARGS=(--tags --no-recurse-submodules)
    if [[ $(git -C KernelSU rev-parse --is-shallow-repository) == true ]]; then FETCH_ARGS+=(--unshallow); fi
    git -C KernelSU fetch "${FETCH_ARGS[@]}" "$BAKASU_URL" '+refs/heads/main:refs/remotes/origin/main'
    SELECTED=$(git -C KernelSU rev-parse refs/remotes/origin/main)
else
    SELECTED=$BASELINE
fi
git -C KernelSU merge-base --is-ancestor "$BASELINE" "$SELECTED" || die 'Upstream history diverged from the validated baseline'
if ((!REPACK_ONLY)); then
    [[ -x $CLANG_DIR/bin/clang ]] || die 'Pinned Android Clang is missing; see docs/BAKASU_BUILD.md'
    for command in make bison flex bc aarch64-linux-gnu-gcc arm-linux-gnueabi-gcc; do
        command -v "$command" >/dev/null || die "Missing $command (see docs/BAKASU_BUILD.md)"
    done
    git -C KernelSU checkout --detach "$SELECTED"
fi
mkdir -p "$ROOT/artifacts/boot-builds"
RUN_DIR=$(mktemp -d "$ROOT/artifacts/boot-builds/$(date -u +%Y%m%dT%H%M%SZ)-XXXXXX")
trap 'printf "Run failed. See %s; no device was flashed.\n" "$RUN_DIR" >&2' ERR
printf 'Run directory: %s\nBakaSU: %s (%s)\n' "$RUN_DIR" "$SELECTED" "$SELECTION"
python3 - "$RUN_DIR" "$SELECTED" "$SELECTION" "$REPACK_ONLY" "$BASE_BOOT" <<'RECORD'
import json,sys
from pathlib import Path
directory,commit,selection,repack,base=sys.argv[1:]
(Path(directory)/'run.json').write_text(json.dumps({
    'bakasu_commit':commit,'bakasu_selection':selection,'base_boot':base,
    'mode':'repack-existing-build' if repack=='1' else 'build-and-repack'},indent=2)+'\n')
RECORD
if ((!REPACK_ONLY)); then
    KERNEL_ARTIFACTS="$RUN_DIR/kernel"
    BAKASU_COMMIT="$SELECTED" BAKASU_SELECTION="$SELECTION" JOBS="$JOBS" CLANG_DIR="$CLANG_DIR" \
        OUT_DIR="$ROOT/out" ARTIFACT_DIR="$KERNEL_ARTIFACTS" bash scripts/devon/build-bakasu.sh
fi
KERNEL_ARTIFACTS=$(realpath -e -- "$KERNEL_ARTIFACTS")
python3 scripts/devon/repack_boot.py --base "$BASE_BOOT" --kernel-artifacts "$KERNEL_ARTIFACTS" \
    --bakasu-commit "$SELECTED" --output "$RUN_DIR/boot.img" --report "$RUN_DIR/boot-manifest.json"
printf '%s\n' "$SELECTED" > "$RUN_DIR/bakasu-commit.txt"
if ((!REPACK_ONLY && !KEEP_OUT)); then
    python3 - "$ROOT" <<'CLEANUP'
from pathlib import Path
import shutil,sys
root=Path(sys.argv[1]).resolve(strict=True);out=root/'out'
assert (root/'scripts/devon/source-pins.json').is_file()
assert out.parent==root and out.name=='out' and out.resolve(strict=True)==out and not out.is_symlink()
shutil.rmtree(out)
CLEANUP
fi
trap - ERR
printf '\nBoot image: %s\nChecksum: %s\n' "$RUN_DIR/boot.img" "$RUN_DIR/boot.img.sha256"
printf 'Ramdisk/header settings preserved; image hashes validated. Boot/runtime behavior is untested.\n'
