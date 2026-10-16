# BakaSU/SUSFS 2.3.0 compatibility audit and first source change

**Historical checkpoint:** Source selection and SELinux ownership have been superseded by the [automatic-hook checkpoint](SUSFS_2_3_SELINUX_AUTOHOOKS.md). Original source pins/results below are preserved as historical evidence.

Date: 2026-10-13 (Asia/Calcutta). Status: pairing audit completed; supported integrated build route remains unresolved. An independent kernel ownership correction and a staged typed SELinux adapter are implemented and host-tested. See the [adapter checkpoint](SUSFS_2_3_SELINUX_ADAPTER.md).

## Historical decision (superseded)

Keep f4435f007b16f61705089ff982e3f981a6017979 as the BakaSU reference and leave its compatibility guard unchanged. Do not select an older revision simply because it lacks the stop. Neither the pre-suspension candidate nor a release tag is certified as a compatible or safe pairing by this audit.

The user authorized starting implementation after the documentation checkpoint. Independent kernel ownership fixes can proceed without claiming a supported BakaSU/SUSFS integration. Integrated builds still require a legitimate resolution of the upstream restriction; no check was removed or skipped.

## Sources obtained and compared

Full Git checkouts, with metadata, were obtained under .backport-context/repos/bakasu and .backport-context/repos/susfs. Both are detached at the exact pinned SHAs, source-clean, and match all supplied reference files (99 BakaSU, five SUSFS). The original snapshots and input manifest remain immutable.

| Reference | SHA / finding |
|---|---|
| Current BakaSU | f4435f007b16f61705089ff982e3f981a6017979 |
| Current BakaSU version code | 35227, independently reproduced from its Git history count |
| Pre-suspension candidate | 5b76b884c75f729a220bb317aa4a4fc78f0e0e9c |
| Build suspension | 48fa4bb7ec8bddb8ea932a8c9be0877f89a1fa73 |
| SELinux behavior change | fa12ffde8244008e9afdb7ac0c1bb1bc1b06b95a |
| Actual v4.2.0-rc3 tag | 239e1e8871b8fcd51a6e5b3002e0ba522fdd99fb; not the failed-build source commit |
| SUSFS 2.3.0 | 82f08351fadb8b19dd6591bd14634d02ad832145 |

The SUSFS reference was committed on October 12 and describes synchronization with official KernelSU commit 0a1f3ff71719fa2d6ec15f5568dac27fdb957fcc. This does not imply compatibility with BakaSU: its reference integration patch targets official KernelSU and differs from BakaSU's non-GKI implementation.

The supplied KernelSU integration patch fails git apply --check against pinned BakaSU, with 24 file applicability failures. It was not applied.

## Exact SELinux mismatch

| Contract | Pre-suspension BakaSU | Pinned current BakaSU | SUSFS reference |
|---|---|---|---|
| fake_state provider | Present | Removed | Kernel hooks and selinuxfs refer to fake_state |
| Original SID/error outputs in context conversion | Absent | orig_sid_p and orig_rc_p preserved | KernelSU patch removes them from the exposed signature |
| Paired context validation | Older behavior | Validates backup and live-policy contexts together | Supplied KernelSU patch removes the paired validation call from the hook path |
| Build restriction | No unconditional stop | Unconditional stop | No BakaSU-specific resolution supplied |

Relevant pinned source locations:

- BakaSU kernel/feature/selinux_hide.c:175-177,1215-1222: current context-conversion contract.
- BakaSU kernel/feature/selinux_hide.c:479-495: paired context validation, real SID/error output and return handling.
- SUSFS kernel_patches/KernelSU/10_enable_susfs_for_ksu.patch:651-656: replacement signature removes original outputs.
- Same patch:695: adds fake_state for the older kernel branch.
- Same patch:863-866: removes the original paired validation call.
- SUSFS kernel_patches/50_add_susfs_in_gki-android12-5.10.patch:2554,2579 and 2597 onward: kernel consumers of fake_state.
- BakaSU kernel/tools/susfs_compat.mk:20: unchanged stop.

The suspension commit cites divergence in SELinux behavior. The subsequent change addresses a timing side channel and refactors non-GKI compatibility. Matching function names or restoring a dummy fake_state would not preserve this behavior.

Motorola's SELinux layout adds another real adaptation requirement: selinux_state has an ss pointer, and status_lock/status_page reside in selinux_state.ss. Its ss->sidtab is a pointer. The 5.10 patch's direct state status_lock access cannot be copied unchanged.

The published v4.2.0-rc3 tag and the pre-suspension candidate predate the context-validation fix. Removing the restriction by choosing them without an explicit regression assessment is not the selected route.

## Required forward integration

A forward integration must preserve the current BakaSU context parser and permission/transition checks. Prefer typed public adapter functions over exposing obsolete state globals:

1. Define a kernel/BakaSU context-conversion interface that retains both backup-policy and real-policy SID/error results. Reuse the current paired parser; do not fall back to an early-return fake-policy-only check.
2. Define canonicalization and access-decision interfaces with explicit backup-policy lifetime and locking. Review interaction with policy reload, feature enable/disable and backup destruction.
3. Adapt the kernel setprocattr hook so the genuine transition uses the real SID and preserves SETCURRENT, bounded-transition, DYNTRANSITION and ptrace checks.
4. Adapt selinuxfs context/access/status paths to the actual 4.19 state/ss layout. Preserve original permission checks and ordinary syscall results.
5. Ensure the compatibility feature probe selects the implementation actually compiled. The current name-based probe must not suppress automatic hooks in favor of incomplete inline hooks.
6. Validate native/compat calls, invalid/valid contexts, policy reload and concurrent feature toggles. Timing behavior requires measurement; source review or compile cannot prove its absence.
7. Retain the unconditional stop until an actual supported integration is available. This audit does not authorize deleting it or declare that the new interfaces are implemented.

Official main, recent open pulls, recent issues and published release tags were inspected; no suitable updated BakaSU integration was established from those sources. This is not a claim that every external fork was searched.

## First implemented kernel correction

Only fs/exec.c was changed. The existing manual-hook wrapper called __do_execve_file and then passed filename to the BakaSU post-exec wrapper. __do_execve_file consumes the filename on both ordinary success and error cleanup; BakaSU's post wrapper dereferences filename->name.

The wrapper now retains the post-prehook filename, increments its existing reference count while valid, calls the consuming exec implementation, invokes the post hook while the filename is alive, and releases the retained reference afterward. Invalid/error filenames do not receive a retained reference or post hook. The retained pointer is separate from the hook's mutable pointer argument.

The manual/SUSFS configuration selection is not broadened by this change. The correction is needed by the later inline integration; it does not introduce SUSFS 2.3.0 or enable its features.

A nested scope keeps the declaration before statements and preserves the existing legacy patch context. The first draft placed the declaration in that context and caused a patch-check failure; it was revised. The unchanged legacy patch now passes strict applicability checking against the corrected source.

## Executed validation and limits

scripts/susfs/test_exec_filename_lifetime.py extracts the actual wrapper from fs/exec.c and compiles it with mocks for the consuming exec routine and hook callbacks. It also extracts the old wrapper directly from the pinned kernel Git commit as a negative control. Tests do not duplicate a hand-written copy of the corrected wrapper.

Host Clang 21.1.8 was installed in Ubuntu WSL with make, libc development headers and sanitizer runtime. This is not the prescribed Android Clang toolchain, and no ARM64 kernel translation unit was built.

| Test | Actual result |
|---|---|
| Old wrapper negative control | ASan heap-use-after-free reproduced |
| Corrected wrapper, manual hook | Nine cases passed under ASan/UBSan |
| Corrected wrapper, hook disabled | Nine cases passed under ASan/UBSan |
| Cases | Success, ENOMEM/EAGAIN/ENOENT cleanup, input EFAULT, pre-hook replacement/failure, post-hook pointer mutation, existing audit-style reference |
| Reference balance | Allocated/freed counts and surviving external reference checked |
| Legacy device patch after correction | git apply --check --whitespace=error-all passes |
| Current unmodified compatibility fragment | make exits 2 at the intentional stop |
| KernelSU reference patch on BakaSU | git apply --check exits 1, with 24 file failures |
| Full kernel compile/link, boot, runtime | Not performed |

These host results establish wrapper ownership behavior under modeled calls. They do not establish correctness of BakaSU internals, ARM64 code generation, LTO/CFI, the complete kernel, or a bootable image.

Logs and machine-readable results are preserved under .backport-context/audit/, including bakasu-pairing-results.json, bakasu-guard-check.log, bakasu-reference-apply.log and exec-lifetime/results.json. Both pinned reference checkouts remain source-clean.

## Next implementation boundary

The exec ownership correction and [typed SELinux adapter](SUSFS_2_3_SELINUX_ADAPTER.md) are ready for source review. The adapter preserves paired validation, supplies coherent callers/providers and tests policy/page lifetime contracts with host mocks. Full-tree policy lifecycle review, legitimate ARM64 build validation and the other SUSFS interfaces remain outstanding. The full integration remains explicitly blocked by the preserved BakaSU restriction.

No push, CI dispatch, image repack/release, flashing or device operation occurred.

Primary references:

- [BakaSU build suspension](https://github.com/Baka-SU/BakaSU/commit/48fa4bb7ec8bddb8ea932a8c9be0877f89a1fa73)
- [BakaSU SELinux change](https://github.com/Baka-SU/BakaSU/commit/fa12ffde8244008e9afdb7ac0c1bb1bc1b06b95a)
- [Original KernelSU change](https://github.com/tiann/KernelSU/pull/3877)
- [SUSFS integration patch](https://github.com/ShirkNeko/susfs4ksu/blob/82f08351fadb8b19dd6591bd14634d02ad832145/kernel_patches/KernelSU/10_enable_susfs_for_ksu.patch)
