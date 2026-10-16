# SUSFS 2.3.0 backport implementation and validation plan

Current checkpoint, 2026-10-15: [all eight optional feature implementations](SUSFS_2_3_ALL_FEATURES.md)
now have conditional consumers and capability output. Direct overlay iteration,
mount-ID ownership, namespace/proc/statfs/statx consumers and command layouts
have source/host coverage. Explicit individual/all build profiles retain the
pinned BakaSU provider and CFI/LTO. Full all-eight and core-only current-source
Image/dtbs/modules builds pass, along with 65 positive host configurations and
ARM64/ARM32 payload probes. Style cleanup and independent review remain;
boot and native runtime are separate, unpassed gates. The older
stage descriptions below retain the development history.

Updated: 2026-10-14 (Asia/Calcutta). Target: Moto G32 devon, Motorola Linux 4.19.325 non-GKI.

This plan follows the [handoff](../BACKPORT_HISTORY.md), [verified input provenance](SUSFS_2_3_PROVENANCE.md) and [initial interface matrix](SUSFS_2_3_API_MATRIX.md). The user authorized staged implementation. The exec ownership correction is retained. The [automatic-hook checkpoint](SUSFS_2_3_SELINUX_AUTOHOOKS.md) selects the maintainer's e1682a2 test branch plus the SID-hash follow-up, rebases policy/allocation fixes, and supersedes the former typed inline SELinux adapter. The [task-state checkpoint](SUSFS_2_3_TASK_STATE.md) adds validated UID transitions, credential publication and one-shot SU exec state. The [access/stat checkpoint](SUSFS_2_3_ACCESS_STAT.md) adds checked filename consumers with explicit reference/error contracts. The [read/init.rc checkpoint](SUSFS_2_3_INIT_RC.md) adds the synchronous FD view and final-close lifetime contract. The [reboot/input checkpoint](SUSFS_2_3_REBOOT_INPUT.md) adds the SUSFS transport guard and accepted-press safe-mode cutoff. The [core foundation](SUSFS_2_3_CORE_FOUNDATION.md) adds 4.19 monitor lifetime, core query/log dispatch and limited target payload layout checks. The [path core](SUSFS_2_3_PATH_CORE.md) adds validated registration, deferred SRCU iteration and typed marker providers. The [consumer checkpoint](SUSFS_2_3_PATH_CONSUMERS.md) adds guarded native lookup/cache and directory consumers. The latest [FUSE live index](SUSFS_2_3_FUSE_PATH_INDEX.md) closes the tested live-inode/dentry-cache gap without a physical /data translation. The production gate remains closed for the remaining fallback/stacked-filesystem coverage and metadata-race review. No build or runtime test is passed unless PORT_LOG.md records its actual result.

## Constraints and completion criteria

Protect devon-resukisu and its tested recovery artifacts. Work only in the isolated backport branch. Do not bypass BakaSU compatibility checks, fake version numbers, push, publish/release or flash images. Device testing requires a separate explicit decision and recovery preparation.

A successful compile is one gate. Completion requires a coherent command ABI, correct hook and mount semantics, reproducible source/config/toolchain inputs, reviewed memory and locking behavior, and explicitly recorded temporary-boot/runtime results on the exact device and ROM.

The selected development source's unmodified SUSFS compatibility fragment accepts the intended non-GKI automatic SELinux route. This resolves the earlier general suspension for that pinned test source; it does not certify the complete pairing. The separate unchanged upstream inline-hook check now finds every required name and passes, with manual-guard warnings retained. Actual-source host tests exercise the new reboot/input consumers; Feature core/consumers, remaining payload ABI and integrated kernel compilation remain incomplete; the staged core foundation explicitly rejects unported feature configurations. All compatibility checks remain in place.

## Source and change organization

Use 18bd9752ada79090ce11326662eaaa9f33717728 as the isolated branch base. The legacy 1.5.9 patch remains an immutable historical reference; it must not be applied and then augmented with overlapping 2.3.0 mount logic.

Selected implementation organization: review direct source changes in small commits on the new branch, with CI consuming those committed sources. Once that strategy is adopted, remove legacy patch application and upstream file overwrites from that branch's SUSFS build path in a deliberate workflow change. Do not combine direct source integration with an automatically applied second copy of the patch.

If the project instead selects a generated device patch as the authoritative deliverable, generate it from a clean pinned base and verify its reconstruction in a separate clean worktree. Never maintain two independently edited implementations. This storage choice must be settled before the first kernel patch.

During intermediate development, mark incomplete feature configurations as incomplete. Do not add stubs, misleading feature output or relaxed checks merely to obtain a green build. Keep documentation of which configurations are intended to build at each checkpoint.

## Stage 0: Preserve evidence — completed for this checkpoint

Create the isolated susfs-2.3-backport worktree; preserve the original handoff byte-for-byte, selected snapshots, ZIP and diagnostic log. Record source pins, manifest hashes, exact patch-check failures and the limits of historical device reports.

Validation: legacy applicability, all ZIP comparisons, all supplied Motorola snapshot reconstruction and pinned BakaSU/SUSFS Git blob identity passed. The 5.10 applicability failure is recorded as an expected negative result. Final documentation scope and protected-ref checks must be recorded before delivery.

## Stage 1: Select and adapt BakaSU — source/host checkpoint complete; integrated build incomplete

The selected development pairing is recorded in [source-pins.json](../scripts/susfs/source-pins.json): BakaSU e1682a2831142e68fa30bab97d5690cf90b75119, SUSFS 82f08351fadb8b19dd6591bd14634d02ad832145 and the now 16-file devon provider patch incorporating the required d7260ca hash fix. Exact source-clean checkouts are preserved separately from the original f443 reference. On 4.19, BakaSU owns SELinux interception; native Motorola hooks.c/selinuxfs.c are restored and the earlier inline adapter is archived reproducibly. The [new checkpoint](SUSFS_2_3_SELINUX_AUTOHOOKS.md) records executed validation and remaining source/kernel gates.

Inspect Kbuild, setup.sh, compatibility probes, userspace command definitions and runtime sources. Compare the included 10_enable_susfs_for_ksu.patch hunk by hunk rather than applying it blindly. Compare any candidate replacement revision against f4435f007b16f61705089ff982e3f981a6017979 and document changed APIs and behavior.

The accepted route must be a supported pinned revision or a substantive compatibility integration with evidence and meaningful checks. Do not remove/skip susfs_compat.mk, change hook mode to evade the gate or rewrite a version string to claim compatibility.

Validation gate: all command/hook signatures have providers, callers and feature guards; command transport and userspace tool revisions are known; the intentional upstream restriction is satisfied through a legitimate supported integration. If no such route exists, integrated compilation remains blocked. Document the blocker without claiming implementation completion.

## Stage 2: Specify ABI, flags and lifetime contracts — task-state source/host subset complete

The [task-state checkpoint](SUSFS_2_3_TASK_STATE.md) records executed UID/fork/exec and credential-readiness checks. Command constants are preserved from 2.3.0, but payload ABI and real kernel behavior remain unvalidated.

Freeze command constants, structs, copy directions and error conventions against the selected userspace tool. Decide native/compat-call behavior. Specify task-state inheritance and clearing across fork/clone/exec, UID changes, root transitions, normal/isolated/WebView Zygote and multiple users.

Specify allocator ownership, borrowed namespace IDs, mount-root reference returns, inode/mapping marker lifecycle and private stat result-mask behavior. Decide whether to backport statx mount-ID UAPI coherently or expose an explicit 4.19 limitation. Prefer a separate reviewed UAPI change if full mount-ID support is required; an added internal field is insufficient.

Validation gate: target sizeof/offsetof checks for every payload and userspace match; boundary/error tests defined; flag-width/collision/mask checks; vendor vfsmount/stat layout and module ABI impact reviewed. Definitions and consumers change coherently rather than as disconnected prototype updates.

## Stage 3: Port core infrastructure and inline integration

Port fs/susfs.c with mutex/RCU/SRCU, static-key and workqueue semantics intact. Adapt the SD-card monitor to this kernel's fsnotify handle_event API, free_mark requirement and mark/group/path/inode reference model. Preserve deferred cleanup outside fsnotify's SRCU callback. Review setup_selinux, ksu_cred readiness, boot-completed scheduling, watch failures and missed unlock events.

Correct exec filename lifetime, getname/putname ownership, access/stat filename types, reboot command entry, init.rc fstat-size/read handling, UID hook placement and input entropy duplication. Preserve manual mode in the new branch without altering the protected branch.

Validation gate: actual compiler preprocessing and prototype checks for each hook mode; object/symbol references where normal build permits; normal syscall errors and credential checks preserved; no sleep in atomic/RCU-only contexts; matched initialization/cleanup on every path. The build restriction is not suppressed for these checks.

## Stage 4: Replace mount management as one reviewed series

Adapt allocation, vfs_create_mount, clone_mnt, copy_tree/copy_mnt_ns, mount lookup, propagation, teardown and unnamed-device allocation to Motorola's fs_context architecture.

Remove the old separate SUSFS IDAs, backup IDs and Zygote renumbering paths where replaced. Preserve Motorola vfsmount.data and KABI layout. Define shared ID ranges, ownership flags, exhaustion behavior and clone/error-unwind semantics. Implement bounded non-SUS mount helpers with explicit owned-reference contracts.

Validation gate: review each allocating/freeing path; meaningful tests for allocation failures, repeated create/destroy, bind and recursive bind, shared/slave/private propagation, namespace clones, unshare/setns, detached and self-parent root mounts. Track leaks/double-free/use-after-free risks with debug instrumentation on a suitable test target. Virtual/QEMU tests do not certify vendor device behavior.

## Stage 5: Integrate filesystem features in dependency order

Integrate mount/procfs views, path hiding, kstat/statfs/FUSE, uname/cmdline and SELinux. Preserve RCU/ref-walk fallback, negative dentries, lookup retries, permissions, pathname ownership and vendor inode_permission2 calls. Replace old inode-state helpers consistently.

Keep open redirect and SUS_MAP as later reviewed feature patches. Document disabled features as an intentional staged configuration and ensure querying capabilities reflects the actual implementation. For non-GKI cmdline behavior, adapt fs/proc/cmdline.c; do not create a fake bootconfig interface merely to satisfy patch context.

Validation gate: affected syscall/procfs behavior agrees for root, allowed-su apps, ordinary apps, isolated services, WebView and multiple Android users. Cover native/compat callers; readdir/getdents/getdents64, symlinks/readlink, stat/lstat/fstatat/statx/statfs, fdinfo/inotify, maps/smaps/map_files/pagemap/mem and ptrace/process-memory permission paths as applicable. Verify VMA progression and 4.19 remote-GUP arguments.

## Stage 6: Reproduce builds and retain diagnostics

Update only the new branch's workflow to use the selected authoritative sources and exact BakaSU/SUSFS/setup/toolchain pins. Remove the old 1.5.9 core/header overwrite from the 2.3.0 path. Record source hashes after integration as well as Git SHAs, because a base commit SHA alone does not describe applied source modifications.

Use the actual vendor bengal-perf_defconfig plus moto-bengal.config and devon-default.config, followed by olddefconfig. Validate CONFIG_KSU=y, CONFIG_KSU_SUSFS=y, CONFIG_KSU_DEVON_SELINUX_POLICY=y, manual/tracepoint mode off for SUSFS, CONFIG_64BIT=y, CONFIG_THREAD_INFO_IN_TASK=y and required kallsyms/FUSE/debug settings. Preserve the production LTO/CFI and compat configuration in final build validation.

Capture full configure/compiler/link logs with reliable pipeline exit status, final config, warnings, source/diff manifest and symbol evidence on success and failure. The existing failure uploader omits the BakaSU commit, final config and complete build log.

Validation gate: meaningful matrix builds for KSU-off, manual baseline on the isolated branch, minimal intended SUSFS and final features using Android-16 Clang r547379. Build Image, device trees and modules; review vmlinux symbols, hook call paths, warnings, structure/module ABI and final config. Every executed build receives its own recorded result. No CI dispatch or image operation is authorized by this documentation checkpoint.

## Stage 7: Independent device-validation decision

Only after earlier gates pass should a separately approved temporary-boot procedure be prepared. Preserve the exact Axion 2.8 original and recovery route. Do not use the handoff's ROM/device assumptions without checking the current phone state.

Any later repack must verify the exact original boot-image SHA-256 1632d107116564f6130b8633c5c21b09261009a03712e086dd22cb9e71edc4de and independently check header, ramdisk, AVB/layout, payload size and artifact provenance. This checkpoint does not create or release an image.

Validation gate after explicit authorization: temporary boot, adb availability, encrypted-storage unlock, root/manager/daemon lifecycle, SELinux behavior, Zygote/app isolation, mount/filesystem operation, module load, Wi-Fi/audio/camera and other hardware, suspend/resume and sustained stress. Compare against the verified manual baseline, collect crash/reboot logs and retain rollback capability. Never treat compile or temporary boot alone as runtime success.

## File-by-file migration map

| Area / files | Planned change | Principal validation |
|---|---|---|
| include/linux/susfs.h, susfs_def.h | 2.3.0 command/layout/flag definitions with real 4.19 adaptations | Target/userspace ABI and capability checks |
| fs/susfs.c, fs/Makefile | Core, registration, synchronization, SD-card monitor | Feature guards; fsnotify/error/refcount tests |
| fs/namespace.c, fs/mount.h, include/linux/mount.h | Mount IDs, cloning/lookup/helpers, vendor layout | Ownership, propagation, namespace and module ABI |
| fs/super.c | Unnamed-device minor allocation | Range/exhaustion/free consistency |
| fs/exec.c, fs/open.c, fs/stat.c | Correct inline BakaSU calls and filename lifetime | Native/compat success/error/permission paths |
| fs/read_write.c, kernel/sys.c, kernel/reboot.c | Init.rc, UID/task-state and command transport | Root/daemon lifecycle and genuine syscall behavior |
| drivers/input/input.c | Pinned handler integration; remove duplicate entropy call | Atomic context, volume-down and key deactivation |
| fs/namei.c, fs/dcache.c, fs/readdir.c | Reconcile old hooks; new lookup/hiding behavior | RCU walk, permission, retries and directory consistency |
| fs/statfs.c, include/linux/stat.h, include/uapi/linux/stat.h | Spoof wrappers; explicit statx decision | Stat/statfs/proc agreement; layout/copyout/compat |
| fs/proc_namespace.c, proc/fd.c, notify/fdinfo.c | Mount, fd and notification metadata | Namespace views and helper references |
| fs/proc/base.c, proc/task_mmu.c, mm/memory.c | Redirect/SUS_MAP and memory interfaces | VMA/locking/GUP/permission behavior |
| fs/proc/cmdline.c | Non-GKI command-line interface | Bounds, output consistency and config |
| security/selinux/avc.c, hooks.c, selinuxfs.c | Pinned BakaSU/SUSFS SELinux hooks | Actual compiled mode, SID readiness and policy behavior |
| kernel/kallsyms.c | Reconcile symbol filtering | Debug visibility and honest feature reporting |
| arch/arm64 thread_info plus task-state callers | Verify flag storage/masks/inheritance | Fork/exec/UID and native/compat behavior |
| Pinned BakaSU full tree | Validated interface integration, checks retained | Supported pairing and actual selected objects |
| .github/workflows/build-devon.yml | Single source authority; pins and diagnostics | Reproducible build and failure artifacts |

## First reviewable change and outstanding decisions

The selected source/host checkpoint uses BakaSU's upstream non-GKI automatic SELinux route without a downgrade. The independent exec correction and policy/allocation fixes are retained, and the previous adapter remains reproducible as historical evidence. The validated task-state/UID and SU exec checkpoint is complete at source/host level. Access/stat filename integration now passes its source/host checkpoint. Read/init.rc/fstat now passes its staged synchronous FD-view source/host checkpoint. Reboot/input integration now passes its source/host checkpoint. The core foundation adds initialization, query/log dispatch, a retrying 4.19 fsnotify monitor and five target payload-layout probes. The isolated path-core subset now passes source/host validation. The next boundary is path lookup/dcache and readdir consumers before SUS_PATH enablement, then remaining feature core/payload ABI, coherent mount ownership and filesystem consumers. Command/compat ABI and statx mount-ID semantics must be settled before those consumers are enabled. The workflow remains the old integration and must be deliberately replaced before it can validate this source. No check may be suppressed or capability/version falsified.

Append future dated entries to [PORT_LOG.md](PORT_LOG.md), recording exact sources, changes, commands/exit codes, negative results, rationale and separate source-review/build/boot/runtime status.

## Current boundary: initial path consumers ported; enablement still pending

The guarded native namei/cache and all five readdir/getdents emitter paths are implemented and host-tested. No fake-name substitution, global dcache mutation or new native structure field is used. Hidden RCU hits retry in reference mode, native references and permission APIs are preserved, and filtered tasks resolve cold opens through the existing separate lookup/create fallback before open/truncate effects.

Next resolve cache-independent FUSE enumeration with a real inode identity contract (not an assumed /data inode-number translation), audit fallback behavior on atomic-open filesystems and stacked/overlay paths, and complete the missing native/concurrent integration checks. Then review removal of the aggregate SUS_PATH rejection and truthful capability advertisement. See [consumer contracts and evidence](SUSFS_2_3_PATH_CONSUMERS.md). The remaining feature bodies/ABI, mount-ID/namespace series, pinned ARM64 build workflow and separately authorized device validation still follow.

## Current boundary after the FUSE index checkpoint

The live FUSE reported-inode index now covers the documented dentry-cache miss for a still-live marked inode. It follows native attribute/owner updates, excludes other superblocks and drops entries before mapping teardown/free. It holds no inode/mount references and appends only a guarded private FUSE field. Plain, plus, cached and backing parser consumers and native/compat records pass host checks.

Next review the separate open/create fallback and stacked/overlay identities, concurrent name/mount traversal, and duplicate/reported-metadata limits before production PATH enablement/capability output. Keep the remaining feature ABI/core, mount-ID/namespace, reproducible ARM64 build and separately authorized device stages in their existing order. This source/host checkpoint does not establish kernel boot safety.

## Current boundary after open/create and overlay identity checks

The guarded lookup_open handoff now rejects an inode marked during successful creation before transferring the path. The host fixture executes the native atomic-open helper and checks native error, notification and reference behavior. Native overlay identity/copy-up helpers pass isolated view-marker tests. Overlay directory cache misses explicitly remain visible because the native inode cache key is a backing pointer, not the emitted directory inode number; generic ilookup cannot supply that identity contract.

Next implement a justified cache-independent overlay enumeration identity/lifetime contract and audit the generic fallback on other supported filesystems. Remote-create effects, concurrent name/mount/owner/marker changes, reported-metadata ambiguity and native integration remain open. Production SUS_PATH rejection and capability exclusion stay in place. See [checkpoint evidence and limits](SUSFS_2_3_OPEN_CREATE_OVERLAY.md). Remaining feature ABI/core, mount-ID/namespace work and the pinned SUSFS ARM64 build still follow; device validation remains separately authorized.

## Current boundary after merged-overlay enumeration

Merged overlay entries now resolve through native view lookup before per-caller filtering, including dentry-cache misses. The shared impure/translated inode-number cache uses a validated internal metadata lookup and retains caller-independent native identity. Ordinary lookups still filter marked views; inode ownership and BakaSU checks remain unchanged. The predicate/helper have GPL exports for overlay module consumers, with source/header syntax evidence only.

Next solve cold direct/translated backing-directory entries without recursive lookup under the backing directory lock. Failed per-entry lookups, readable-but-unsearchable directories, nested policy layers, native concurrency and real lookup cost remain open. See [merged-overlay evidence and limits](SUSFS_2_3_OVERLAY_MERGED.md). Keep production SUS_PATH rejection/capability exclusion and the remaining feature ABI/core, mount-ID/namespace, pinned ARM64 and separately approved device stages in their existing order.

## Build milestone: limited ARM64 preparation complete

The user selected an early core-only target compilation before completing every optional feature. The [limited build recipe](SUSFS_2_3_TEST_BUILD.md) now pins and stages the reviewed BakaSU provider, integrates its native driver/Kconfig, generates the real Motorola configuration and passes native Kbuild checks. Only SUSFS core/query/logging is enabled; all incomplete feature flags remain disabled and the production rejection/capability logic is unchanged. A separate manual-only workflow uses the same driver, while the immutable legacy workflow remains historical evidence.

Next run ./build-susfs-test.sh --build for Image, dtbs and modules. No source/preparation task remains mandatory before that first limited attempt; target compilation may expose additional issues. The full feature/ABI, direct-overlay, native concurrency and mount-ID/namespace work above remains required for a fuller SUSFS kernel. No target build, boot-image packaging, boot or runtime validation is claimed by preparation.

## Build milestone: first limited ARM64 compile/link passed

The subsequently authorized --build run now passes for Image, dtbs and modules. Three bounded fixes resolved the native uppercase built-in KSU representation and missing credential/private filesystem declarations. Run 20261015T090201Z-18cxdmw2 produces a raw ARM64 Image, 66 modules and four device-tree files, with existing CFI/LTO and compatibility checks intact. [The first-build record](SUSFS_2_3_FIRST_ARM64_BUILD.md) contains exact attempts, fixes and artifact validation. The protected working reference and separate non-SUSFS project remain unchanged.

This completes the first limited compilation milestone: BakaSU plus SUSFS core/query/logging, with all eight unfinished optional features disabled. Next continue the guarded direct-overlay/path identity and native concurrency work, remaining feature bodies/command ABI and mount-ID/namespace consumers before enabling the corresponding capabilities and repeating ARM64 builds. The limited image has not been repacked into boot.img, booted or runtime-tested; device validation still needs separate approval. Compilation alone does not establish boot safety or a complete SUSFS backport.
