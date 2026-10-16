# First limited devon SUSFS ARM64 build

Current follow-up: [the eight-feature experimental port](SUSFS_2_3_ALL_FEATURES.md)
adds explicit individual/all profiles. The default remains core-only; the
older incomplete-feature and aggregate-gate descriptions below record the
first-build checkpoint. They do not describe the current all-feature sources.

Date: 2026-10-15. Build preparation for the existing susfs-2.3-backport checkout, based on kernel 18bd9752ada79090ce11326662eaaa9f33717728. The separate LineageOS 23.2 BakaSU checkout and its boot-image tools are unchanged.

Follow-up: [first ARM64 build attempts and fixes](SUSFS_2_3_FIRST_ARM64_BUILD.md). Target compilation was subsequently authorized. This recipe includes the resulting header and native uppercase-built-in fixes; the preceding preparation milestone remains historical evidence.

The first limited target build now passes after three documented compatibility fixes. Run 20261015T090201Z-18cxdmw2 produced a raw ARM64 Image, 66 modules and four device-tree files. This does not finish every SUSFS feature. The guarded [merged-overlay checkpoint](SUSFS_2_3_OVERLAY_MERGED.md) and other unfinished consumers remain intact and disabled in this profile.

## Run in WSL

```sh
cd ~/projects/android_kernel_motorola_sm6225-susfs-2.3-backport
./build-susfs-test.sh --prepare
./build-susfs-test.sh --build
```

The default is --prepare: verify inputs, prepare a separate pinned BakaSU dependency, generate the real Motorola configuration and evaluate native BakaSU/Kbuild checks. --build explicitly adds Image, dtbs and modules compilation with four jobs. --jobs N changes parallelism; --clang-dir PATH accepts only the pinned compiler fingerprints. Each invocation uses a fresh directory under out/susfs-core-test/ and a matching evidence directory under artifacts/susfs-core-test/. Preparation can be repeated safely. No prior build or kernel artifact is used as a fallback.

No boot.img is generated or modified. The command performs no adb, fastboot, push, release or CI dispatch. Output directories remain available for diagnosis. Successful target compilation establishes neither bootability nor runtime safety.

## Enabled scope

The [profile fragment](../arch/arm64/configs/vendor/ext_config/devon-susfs-core-test.config) is merged after the unchanged Motorola bengal-perf, moto-bengal and devon-default configuration inputs. It selects built-in KSU, the SUSFS route, the checked devon SELinux provider, the SUSFS core and logging. The kernel's existing LTO/CFI, devon device-tree selection, SELinux, module support, task-flag prerequisites, fsnotify and SRCU must remain enabled.

All eight incomplete feature flags are explicitly disabled: SUS_PATH, SUS_MOUNT, SUS_KSTAT, SPOOF_UNAME, HIDE_KSU_SUSFS_SYMBOLS, SPOOF_CMDLINE_OR_BOOTCONFIG, OPEN_REDIRECT and SUS_MAP. The script also rejects future unknown enabled KSU_SUSFS_* feature flags. The production aggregate rejection remains in fs/susfs.c; nothing removes it to make this build pass. Capability output remains truthful: logging is the only implemented optional feature advertised by this profile. The core's unsupported AVC toggle continues returning EOPNOTSUPP.

This is a core/query/logging and BakaSU-hook compilation experiment, not a path/mount-hiding test kernel. The native task, exec, access/stat, synchronous init.rc, reboot/input and automatic SELinux integrations are included as selected by that route. Their native ARM64 behavior remains unvalidated.

## Exact source/toolchain integration

[build-pins.json](../scripts/susfs/build-pins.json) records 29 reviewed kernel/configuration inputs, all 16 patched BakaSU file hashes, the provider patch hash, exact source commits, toolchain archive/binary fingerprints and action references. [The build driver](../scripts/susfs/build_test.py) checks these before configuration and again before compilation. Unexpected kernel source changes require deliberate review and refreshed pins, rather than silently reusing an older configuration.

BakaSU is pinned to e1682a2831142e68fa30bab97d5690cf90b75119 with the authoritative 16-file devon provider patch (SHA256 d75748e5d0a054d7ac49e56b7218f1d44ec4c6ed7ed091088469c40f7e78493c). Its first-build follow-up accepts the native uppercase built-in representation without accepting module mode. An independent fetch confirmed the exact commit remains available from the official repository. The moving sync-susfs branch is not selected automatically. The SUSFS 5.10 reference remains 82f08351fadb8b19dd6591bd14634d02ad832145; this script never copies its unported source over the local port.

A separate dependency checkout is created under .backport-context/build/. [drivers/kernelsu](../drivers/kernelsu) is a relative symlink to that checkout's kernel directory; drivers/Kconfig and Makefile use the native integration entries. Existing pristine audit checkouts are not patched. A fresh clone uses the local full-history reference when available or the official remote otherwise, explicitly fetching the pinned commit if needed. There is no upstream setup-script fallback to main, no stash/reset of existing edits and no synthetic source version. Existing dependency changes must match exactly the reviewed 16-file provider.

BakaSU retains its native version calculation: the local prepared checkout has 4529 revisions, yielding code 35229 and name v4.2.0-rc3-e1682a28-dirty. The dirty suffix truthfully records the provider patch. Kernel and SUSFS versions and compatibility checks are unchanged.

Android Clang r547379 comes from the recorded official Android 16 archive (source commit 9916fb51ccb914d62d35ad9a7b9b21d2ef046928, archive SHA256 6350d387cbe6d75c1df1018da2a29c6bf1b233bd58e122e44f3ef5b16a90facc). The cached official archive was independently hashed and its critical compiler/binutils members matched the extracted binaries. Missing installations are downloaded from that pinned URL and checked before extraction. An isolated file:// fixture exercises the download/checksum/extraction path without another large download.

Ubuntu host dependencies, already present locally:

```sh
sudo apt-get install bc bison build-essential flex make libssl-dev libelf-dev \
  libncurses-dev cpio rsync ca-certificates python3 perl gcc-aarch64-linux-gnu \
  gcc-arm-linux-gnueabi zstd dwarves
```

The script records host-tool versions, compiler identity, actual .config, source integration diff against the pinned kernel base, provider patch, native Kbuild diagnostics and checksums. Successful compilation must additionally validate the ARM64 Image header, AArch64 vmlinux, linked SUSFS/BakaSU symbol evidence, module outputs and devon device-tree outputs before reporting compiled-and-linked. Image memory size includes BSS and can exceed the file size; the validator accounts for that distinction. Compilation or collection failure retains logs, failed-stage/error metadata and checksums, never a successful-image claim.

## Workflow

[build-devon-susfs-core.yml](../.github/workflows/build-devon-susfs-core.yml) is manual-only, has read-only repository permission, uses full source history and verified exact checkout/upload action commits, and invokes the same local driver. It uploads experimental build evidence and raw kernel outputs; it does not package a boot image or publish a release. It has not been dispatched. Local uncommitted workflow/source changes must first reach a selected remote revision before GitHub can run them; this checkpoint does not commit or push them.

The historical build-devon.yml remains byte-preserved as an immutable handoff input. Its legacy SUSFS 1.5.9 patch/repack route is not the workflow for this 2.3.0 core experiment. The older scripts/susfs/source-pins.json remains a historical staged-port record; build-pins.json is the explicit build recipe for this checkpoint.

## Preparation validation

Actual --prepare runs pass defconfig, fragment merge, olddefconfig, final profile verification and native provider Kbuild evaluation. The real upstream inline-hook checker finds all seven hooks; its existing manual-guard warnings remain visible. Native API detection selects the devon automatic SELinux/pointer-sidtab layout without a fake manual-SELinux marker. The fs object plan contains susfs.o and excludes unfinished path/FUSE consumer objects. Preparation itself runs no ARM64 object or kernel target; the subsequent authorized --build run compiled and linked the limited profile successfully.

```sh
python3 -B scripts/susfs/test_build_preparation.py /absolute/path/to/preparation/evidence
```

This local verification script also uses PyYAML (Ubuntu python3-yaml). It checks the actual prepared profile, eight feature refusals, an unknown future feature, module KSU, missing policy/CFI, source/provider/HEAD/compiler drift, invalid Image headers, corrupt archive handling and workflow structure. Four real Kbuild negative runs reject lowercase/uppercase module KSU, missing SUSFS selection and the wrong kernel version; native uppercase built-in KSU is accepted. The pre-existing 45 positive source/host reports retain matching input hashes after the documented first-build regression reruns.

Preservation, complete recipe/evidence checks and source/host report hashes are recorded under .backport-context/audit/core-build-prepare/. The protected devon-resukisu reference remains 80f0ee4d1ec68838d25c2efeb955d31e90f85dae, independently confirmed against the remote. All 125 immutable handoff/source inputs, clean pinned audit checkouts and the separate non-SUSFS integration remain preserved.

The [first ARM64 build record](SUSFS_2_3_FIRST_ARM64_BUILD.md) now records the failed attempts, three fixes, final successful run and independent artifact checks. Direct-overlay identity, remaining feature/ABI and mount-ID work remain necessary for a fuller SUSFS build. Boot-image packaging and separately approved device testing remain later stages.
