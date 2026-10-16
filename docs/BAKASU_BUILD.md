# Moto G32 devon: clean LineageOS 23.2 + BakaSU build

Date: 2026-10-15 (Asia/Calcutta). The initial validation covered a non-SUSFS ARM64 kernel, DTBs and modules. The subsequently authorized `build.sh` workflow below also creates a boot image from the user's exact Axion baseline. It performs no flash, release, push or CI dispatch.

## Source selection and history

A fresh clone of [LineageOS lineage-23.2](https://github.com/LineageOS/android_kernel_motorola_sm6225/tree/lineage-23.2) starts at c65462bf08da2a8900b98e705bea376de7f7f19c. Work is on devon-bakasu-lineage23.2. The reference is the user's [devon-resukisu branch](https://github.com/Ojas7112008/android_kernel_motorola_sm6225/tree/devon-resukisu), 80f0ee4d1ec68838d25c2efeb955d31e90f85dae, read without checkout/edit. Its Lineage base is 239b9c3887cc (full SHA in source-pins.json). The new upstream tree differs from that base in only three unrelated paths: guamna config, himax modular support and camera-bengal Makefile.

The relevant integration sequence is abfd660d739b (KSU options), 094fcb99e9e5 (exec), 43dfbfdbe113 (access), e19a825304a3 (reboot), and 9fa8f3cf58ac (stat). Workflow iterations 0d91ef1972ff/b02db0299a8e established dispatch/builds; b8eb84cb2e8b and 7dbf846cbfa6 corrected real DTB output paths and output/error checks. c37b120f2b5c refined the BakaSU workflow, and 3831e3f3a775 added ROM-specific boot packaging. Later experimental workflow uploads/removals are not part of this build.

Public Actions metadata confirms a successful [working-branch run on October 14](https://github.com/Ojas7112008/android_kernel_motorola_sm6225/actions/runs/38540605125). Anonymous access to the job log returns HTTP 403, so that run's exact BakaSU SHA cannot be recovered from its logs here. The old workflow accepted a floating main input. This integration instead pins the current inspected main commit d7260ca1e5c128061122924d0d48754e361e135c as a full-history KernelSU submodule. Its source and compatibility checks remain unmodified; the version derives normally from its own Git history.

## Implementation

The standard drivers/kernelsu symlink, Makefile selection and Kconfig source wire in the pinned dependency. A separate devon-bakasu.config fragment enables built-in KSU/manual mode, automatic UID/init.rc/input providers and KALLSYMS_ALL. The native bengal-perf, moto-bengal and devon-default fragments are unchanged. SUSFS and tracepoint mode are not selected; target LTO and CFI stay enabled.

The manual exec/access/stat/reboot calls use a shared typed header. The exec wrapper retains an independent filename reference until the post hook finishes, correcting the original branch's post-consumption use-after-free. The post callback's argument cannot determine which retained reference is released. Native fstat and fstat64 post hooks run only after a successful native result copy. This also gives the required fstat64 name a real guarded caller instead of the reference branch's declaration alone. Other native syscall/credential checks remain.

This is a rebuild of the non-SUSFS integration. It does not import the SUSFS backport, its SELinux adapter, feature flags, mount logic or overlay. BakaSU retains its existing manual-mode semantics and runtime limitations; this compile-oriented task does not certify those hooks, init.rc handling or root behavior on a device.

## Build

Install the host dependencies used by the working workflow: bc, bison, flex, build-essential, libssl-dev, libelf-dev, libncurses-dev, cpio, rsync, curl, ca-certificates, Python 3, dwarves, Perl, gcc-aarch64-linux-gnu, gcc-arm-linux-gnueabi and zstd. The local environment is Ubuntu 26.04/WSL; exact installed versions and compiler evidence are saved with the build outputs. The prior CI used Ubuntu 22.04.

Use Android Clang r547379 from android-16.0.0_r4, whose prebuilt repository commit is 9916fb51ccb914d62d35ad9a7b9b21d2ef046928. The official archive is:

https://android.googlesource.com/platform/prebuilts/clang/host/linux-x86/+archive/refs/tags/android-16.0.0_r4/clang-r547379.tar.gz

Extract it into ../.toolchains/android-16-r547379, or set CLANG_DIR explicitly. Preserve the full BakaSU submodule history so Kbuild derives its genuine version. From this kernel checkout run:

```sh
git submodule update --init KernelSU
JOBS=4 scripts/devon/build-bakasu.sh
```

The script verifies the pinned Lineage base ancestry, exact clean BakaSU revision, Clang version and final configuration. It merges the three Motorola fragments plus the separate BakaSU fragment, then builds Image dtbs modules. --configure-only stops before compilation; --build-only retries against the already validated config. Default parallelism is four to leave memory for linking under WSL. Native LTO/CFI are checked rather than disabled to obtain a build.

The collector verifies the ARM64 Image header, nonempty DTB/module outputs and linked BakaSU symbols. It saves Image, kernel.config, dtbs.tar.gz, modules.tar.gz, linked symbol evidence, the source integration diff/pins, checksums and build-result.json under artifacts/. These are kernel-only artifacts. Build logs and source/config checks are evidence of compilation, not bootability, ROM/module ABI compatibility or safe flashing.

## Validation record

The actual old working-branch exec wrapper is preserved as a regression fixture. The host test reproduces its ASan use-after-free, then passes nine manual-hook and nine no-hook ownership/error cases with ASan/UBSan:

```sh
python3 -B scripts/devon/test_exec_filename_lifetime.py --compiler /usr/bin/clang
```

The local ARM64 `make Image dtbs modules` build completed successfully with Android Clang r547379. Outputs are a 37,519,384-byte Image, four device trees (one base DTB and three Devon overlays), and 66 kernel modules. The linked vmlinux ELF header identifies AArch64. The kernel release is `4.19.325-cip136-st20-perf-gc65462bf08da-dirty`; the suffix records this checkout's uncommitted integration, not a modified BakaSU version. BakaSU reports its native version `v4.2.0-rc3-d7260ca1` / `35229`.

Image SHA-256: `60006a03c23b650c5c7187d272e21ecdb7df13747f967b07507a354f1b34fb46`.

The generated configuration preserves enforcing CFI, ThinLTO and module versioning. Recorded compile commands for the exec/stat/reboot callers and BakaSU sucompat/LSM objects also contain the AArch64, ThinLTO and CFI flags and pinned compiler resource path. The integration's kernel changes have zero checkpatch errors and one new-file MAINTAINERS warning; `git diff --check` passes. BakaSU's manual-hook and kernel-API checks passed without changes to the dependency.

`artifacts/build-result.json`, `SHA256SUMS.txt`, the full build log, linked BakaSU symbol list, configuration and source diff record the completed build. The original checkout's status, protected working-branch reference and SUSFS backport diff were compared with their captured state and are unchanged. No boot.img packaging or device testing was performed. Compilation and linking do not establish bootability, manager/root behavior, runtime hook safety or ROM/module ABI compatibility.

## Build and repack the provided Axion boot image

From WSL, run:

```sh
cd ~/projects/android_kernel_motorola_sm6225-lineage23.2-bakasu
./build.sh
```

The default base is `/mnt/d/Phone expriment/Moto G32/Axion/Last Backup/boot.img`. Use `--base-boot "PATH"` to select a copy of that image; WSL paths and Windows drive paths are accepted. The SHA-256 must be `1632d107116564f6130b8633c5c21b09261009a03712e086dd22cb9e71edc4de`, the same exact 96 MiB Axion image used by the earlier working branch. Other ROM images and signed AVB layouts are rejected.

By default the script fetches the latest `main` from the official BakaSU repository, checks its full-history ancestry against the validated baseline, and checks out the selected commit without discarding local edits. All native BakaSU API/manual-hook checks remain active and its real version is derived from Git. An incompatible future upstream revision fails the build rather than producing a boot image from old outputs. `--pinned` selects the validated `d7260ca1` revision without updating. The effective commit is recorded in each run and kernel artifact manifest. The original `scripts/devon/source-pins.json` remains the validated baseline.

Each run uses a fresh `artifacts/boot-builds/<UTC timestamp>-<unique suffix>/` folder. It contains `boot.img`, `boot.img.sha256`, `boot-manifest.json`, `run.json`, and a `kernel/` folder with Image, modules, DTBs, configuration, build logs, checksums, System.map and module/release metadata. The original input image and earlier build artifacts are not overwritten. Completed runs accumulate; retain the ones needed. The script prints the final image location and never invokes adb or fastboot.

Repacking replaces only the raw kernel. It preserves the original gzip ramdisk (including alignment padding), command line, OS/header settings, AVB properties, flags, salt, rollback metadata and 96 MiB partition size. It recalculates the unsigned boot hash descriptor and footer offsets, then parses and validates the output. It does not disable verification, strip a signature or sign with a new key. The supplied image already uses AVB algorithm NONE.

Temporary build files need about 4 GiB in addition to source/toolchain space while compiling. After successful repacking, `out/` is removed by default; final outputs and small build/debug metadata remain. Use `--keep-out` to retain incremental compilation state. A failed run keeps build files for diagnosis and does not package stale kernel outputs. Default parallelism is four; `--jobs N` or `JOBS=N` changes it.

Other options:

```sh
./build.sh --check                    # inspect base/toolchain and report selected upstream SHA
./build.sh --pinned --jobs 4           # full build using the validated revision
./build.sh --repack-only --pinned      # package the original successful artifacts/ build
./build.sh --repack-only --kernel-artifacts /path/to/a/successful/kernel-artifact-folder
```

`--repack-only` explicitly skips compilation and requires the selected BakaSU commit and artifact checksums/configuration to match a successful recorded build. It does not claim that a new build ran. Use normal `./build.sh` to rebuild.

The repacker derives from the known-working workflow and was checked against the AOSP [boot unpacker](https://android.googlesource.com/platform/system/tools/mkbootimg/+/refs/heads/main/unpack_bootimg.py) and [AVB format/tool](https://android.googlesource.com/platform/external/avb/+/refs/heads/main/README.md). Its real-base tests cover byte-identical roundtrip, kernel growth/shrinkage, payload integrity, partition overflow, corruption, signed-image rejection and protecting the original file:

```sh
python3 -B scripts/devon/test_repack_boot.py \
  --base '/mnt/d/Phone expriment/Moto G32/Axion/Last Backup/boot.img' -v
```

An image passing these checks is a boot-test candidate, not a verified working device image. The base kernel reports release `4.19.325-cip136-st20-perf-g239b9c3887cc-dirty` with Clang 21, whereas this build reports `...gc65462bf08da-dirty` with Clang 20. Stock vendor-module ABI compatibility is not established by kernel replacement; matching newly built modules are retained separately. Device boot, root behavior, module loading and ROM compatibility still require separately authorized testing. No bootloader verification setting is changed by the script.
