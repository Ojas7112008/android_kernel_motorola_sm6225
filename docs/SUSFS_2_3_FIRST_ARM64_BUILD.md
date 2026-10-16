# First limited SUSFS ARM64 build

Date: 2026-10-15. The user authorized running the prepared build and resolving errors without further intervention. Scope remains BakaSU plus SUSFS core/query/logging; unfinished feature consumers remain disabled.

Status: the fresh full ARM64 Image/dtbs/modules build completed with exit zero. The driver reports compiled-and-linked after checking the outputs. This validates the limited compilation/linking profile; it does not establish bootability or working runtime hooks.

## Build failures and fixes

| Run | Result | Diagnosis |
| --- | --- | --- |
| 20261015T085127Z-6qldivmo | Failed before kernel object compilation | The devon provider rejected CONFIG_KSU=Y during native modules.builtin generation. |
| 20261015T085316Z-abtfppto | Failed compiling fs/read_write.o | susfs_def.h used current_uid without including linux/cred.h. |
| 20261015T085652Z-da0vidyk | Failed compiling fs/stat.o | The new filename_lookup caller lacked its existing private declaration from fs/internal.h. |
| 20261015T090201Z-18cxdmw2 | Compiled and linked, exit 0 | Fresh full build after the three fixes and targeted object/subtree validation. |

Each attempt has its own evidence/output directory. Earlier logs and source snapshots are retained. The older driver left failed runs' result.json at its last building state; attempts.json under the checkpoint audit records their actual failures. The current driver now records a failed stage/error and checksums when compilation or artifact validation raises an exception.

The provider guard now accepts exactly lowercase y or uppercase Y for built-in KSU. Linux 4.19 scripts/Makefile.modbuiltin loads tristate.conf, which deliberately represents tristates as uppercase Y/M. Both lowercase m and uppercase M still fail the guard, as do the wrong kernel version and missing SUSFS route. Native build output confirms CONFIG_KSU=Y and the correct built-in modules.builtin entry. No compatibility check is removed and no version is changed.

The authoritative 16-file provider now has SHA256 d75748e5d0a054d7ac49e56b7218f1d44ec4c6ed7ed091088469c40f7e78493c. Only its Kbuild selection guard changed from the preceding provider. A new separate dependency checkout matches that exact patch; the old dependency and pristine pinned references are retained unchanged. build-pins.json and the relative driver symlink were updated explicitly.

The two header fixes preserve runtime policy and function bodies: susfs_def.h adds linux/cred.h, and fs/stat.c includes its existing private internal.h under CONFIG_KSU_SUSFS. They fix declarations using native headers rather than suppressing implicit-declaration errors. The real target build retains its existing warnings-as-errors, CFI/LTO and checked hook configuration. Host fixture header models and exact source-restoration checks were updated for the explicit includes.

## Targeted validation

The diagnostic target invocation uses native ARM64 Kbuild in the third failed output, with normal dependency tracking, to compile fs/read_write.o, fs/stat.o, fs/susfs.o and drivers/kernelsu/. It exits zero. This includes the real provider policy, automatic SELinux, command/transport, exec, init.rc, UID and hook sources. It is object/subtree evidence, not a complete kernel or runtime result. The successful final full attempt uses a fresh output directory rather than treating that diagnostic output as a successful image.

## Full build result

Run 20261015T090201Z-18cxdmw2 produced a 37,519,384-byte raw ARM64 Image, 66 modules and four device-tree files: khaje-moto-base.dtb and the devon DVT2, EVT and PVT overlays. Kernel release is 4.19.325-cip136-st20-perf-g18bd9752ada7-dirty. The Image SHA256 is 8170e2e6073a2e14ffe62fb342d32a7de2f117d2fd78567a326e96b18fff425d. Its header records 39,030,784 bytes of memory, including BSS. The complete compilation/link log contains no compiler warning: or error: diagnostics; the earlier native inline-checker guard warnings remain visible in the separate Kbuild preflight evidence.

The saved [result](../artifacts/susfs-core-test/20261015T090201Z-18cxdmw2/result.json), [build log](../artifacts/susfs-core-test/20261015T090201Z-18cxdmw2/build.log), [configuration](../artifacts/susfs-core-test/20261015T090201Z-18cxdmw2/kernel.config) and [checksums](../artifacts/susfs-core-test/20261015T090201Z-18cxdmw2/SHA256SUMS.json) identify the exact experiment. Raw outputs include [Image](../artifacts/susfs-core-test/20261015T090201Z-18cxdmw2/Image), [modules](../artifacts/susfs-core-test/20261015T090201Z-18cxdmw2/modules.tar.gz) and [device trees](../artifacts/susfs-core-test/20261015T090201Z-18cxdmw2/dtbs.tar.gz). They are not a flashable boot image.

The final independent audit checks every saved checksum, the Image header and vmlinux ELF64 AArch64 identity, linked BakaSU/SUSFS symbols, all module architectures/vermagic/archive bytes and all FDT headers/sizes/archive bytes against this output directory. It also verifies all 45 current positive host configurations against current input hashes, the four native Kbuild refusal cases, the 125 immutable inputs, 32 preserved prior inputs, exact source/provider pins, source whitespace, incremental checkpatch and document links. Evidence and the repeatable auditor are under .backport-context/audit/first-core-build/. Historical failed-run records are retained with their actual outcome in attempts.json.

The checkpoint audit is .backport-context/audit/first-core-build/. It preserves the preceding inputs, full diff, failed build logs, native metadata evidence and targeted compile result. Source commits remain kernel 18bd9752ada79090ce11326662eaaa9f33717728, BakaSU e1682a2831142e68fa30bab97d5690cf90b75119 and SUSFS reference 82f08351fadb8b19dd6591bd14634d02ad832145. Android Clang remains r547379. The protected working reference and separate LineageOS 23.2 BakaSU project are unchanged.

See [the build recipe](SUSFS_2_3_TEST_BUILD.md) for the actual profile, driver and manual-only workflow. No boot image is repacked, released, pushed or flashed. Compilation cannot establish bootability or native hook/runtime safety. All eight unfinished optional SUSFS features remain disabled, and the aggregate production rejection/capability checks remain intact. Full path/mount hiding remains a later implementation and validation stage.
