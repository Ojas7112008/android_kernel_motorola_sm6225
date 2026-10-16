# Devon SUSFS task-state and validated UID checkpoint

**Extension:** The [access/stat checkpoint](SUSFS_2_3_ACCESS_STAT.md) adds owned filename consumers and extends the same provider/shared header. Scope and results below record the preceding task-state stage.

Date: 2026-10-14 (Asia/Calcutta). This extends the [automatic SELinux checkpoint](SUSFS_2_3_SELINUX_AUTOHOOKS.md) in the isolated susfs-2.3-backport worktree. The user authorized beginning the remaining implementation stages. Source and host checks pass; the complete SUSFS kernel remains incomplete and has not been built or booted.

## Exact inputs and scope

Kernel base: 18bd9752ada79090ce11326662eaaa9f33717728, Motorola 4.19.325. BakaSU: e1682a2831142e68fa30bab97d5690cf90b75119, maintainer sync-susfs test branch. SUSFS: 82f08351fadb8b19dd6591bd14634d02ad832145, 2.3.0 for 5.10. The existing d7260ca SID-hash follow-up remains incorporated. Selection is in [source-pins.json](../scripts/susfs/source-pins.json).

The authoritative external [provider patch](../.github/patches/bakasu-devon-selinux-policy.patch) now modifies 16 pinned BakaSU files. Seven files extend the previous nine-file SELinux checkpoint: core/init.c, include/ksu.h, selinux/selinux.c, hook/setuid_hook.c and .h, feature/sucompat.c and .h. Runtime startup and selinux.h also gain checked credential setup. There is no separate independently edited provider implementation. The preceding passing patch, root exec source and affected tests/results are archived under .backport-context/audit/setresuid-task-state/prior-checkpoint.

Root kernel changes are limited to kernel/sys.c, kernel/fork.c, fs/exec.c and two headers. The upstream 2.3.0 susfs_def.h is copied intact apart from standalone includes and a native flag-storage guard. Command numbers and helpers are unchanged; this does not implement their payload ABI or handlers. ksu_susfs.h supplies a shared UID prototype, credential-readiness query and fork helper. Native Motorola SELinux callers still match the kernel base.

## UID and task-state contracts

The setresuid callback runs after namespace UID mapping, credential allocation, capability checks, user-accounting setup and security_task_fix_setuid succeed. It immediately precedes commit_creds, whose actual vendor implementation always returns zero. Keeping the callback before commit preserves BakaSU's old UID/Zygote context and the credentials used by its filesystem operations. The callback's result does not replace the native syscall result.

The target accepts only init_user_ns, a non-sentinel real UID, equal requested real/effective/saved UIDs, and an actual change to one of those credentials. Failed, partial, unequal, sentinel, unchanged and nested-user-namespace calls produce no BakaSU side effects. Ordinary process contexts remain subject to the native checks and BakaSU's own Zygote classification. The actual uid16 adapter reaches the same validated implementation; the 32-bit UID type is preserved. Android multi-user UID ranges are retained.

| State | Contract |
|---|---|
| SUSFS bits 33, 34, 35 | Upstream mount classification, no-SU restriction, Zygote-next classification; inherited across fork and exec |
| BakaSU bit 63 | Existing root-escape restriction; inherited and honored by the target SU exec path |
| BakaSU bit 61 | Pending SU exec marker; cleared in the new child after native thread-state copying, at each new valid exec-hook attempt, and when its post callback consumes it |
| Native ARM64/compat flags | Preserved; private flags do not overlap the actual vendor syscall/work masks |

An allowed Zygote-next transition clears the inherited no-SU bit, matching the ordinary Zygote path, while preserving mount classification and the independent root-escape restriction. Existing upstream ordering is retained: isolated processes and app UIDs selected for unmount are classified before the Zygote-next allowlist check. That ordering can intentionally take precedence over allowlist membership. WebView UID 1053 receives mount classification through ordinary Zygote's unmount path; Zygote-next's isolated/app-only branch sets no-SU for a non-allowed WebView UID without newly setting its mount bits. Existing inherited mount bits are retained. This checkpoint does not invent a different WebView mount policy.

## Credential publication and exec

Target setup_ksu_cred returns a real error for absent credentials, SELinux task-security storage, context-resolution failure or a zero SID. A mutex serializes first setup, and acquire/release publication exposes readiness only after SELinux setup and the existing keyring setup sequence. Subsequent successful setup calls do not mutate published credentials. All three built-in init second-stage paths leave completion unset on credential setup failure and can retry.

Before readiness, a recognized Zygote transition can set the restrictive no-SU latch but cannot clear seccomp, manipulate manager files/FDs, unmount or schedule SUSFS work. Native UID commit still proceeds. SU exec checks readiness before override_creds. This publication gate covers these UID/SU-exec consumers; the complete startup, SID-cache, policy reload and other credential-consuming paths remain to be audited. The existing keyring helper returns void, so readiness is not a claim that every keyring operation was independently validated.

The SU exec path propagates root-profile escape errors before changing the filename or setting its pending marker. It separately rejects bit 63: the pinned escape helper returns zero for that denied case, so treating its return alone as success would be insufficient. The post hook consumes the marker once and requires a nonnegative exec result before installing a scoped SU FD. Failed exec, repeated post callbacks and absent result pointers cannot grant that FD. The filename reference correction is retained and now runs for SUSFS as well as manual mode.

Root-profile group/SELinux/mount transition failures beyond the helper's returned error, other scoped-FD installation paths and real credential/security behavior remain later gates. This is not complete BakaSU startup or root-transition validation.

## Executed validation

```sh
python3 -B scripts/susfs/test_setresuid_task_state.py
python3 -B scripts/susfs/test_selinux_autohooks.py
python3 -B scripts/susfs/test_selinux_adapter.py
python3 -B scripts/susfs/test_exec_filename_lifetime.py
```

| Check | Recorded result |
|---|---|
| 16-file provider reconstruction | Strict apply, reverse applicability, exact file scope and source whitespace pass |
| Upstream definition identity and actual commit_creds return | Source assertions pass |
| UID/task-state host matrix | Three positive configurations pass: KSU off and SUSFS with static keys off/on |
| Native UID validation | Mapping/allocation/capability/accounting/LSM failures, existing restrictions, partial/no-op/sentinel/userns and uid16 paths pass |
| Zygote behavior | Ordinary/next, app/isolated/WebView/multiple users, allow/unmount ordering, manager paths and missing readiness pass under mocks |
| Credential setup | Missing cred/security storage, resolver/zero-SID failures, retry, immutable publication and eight concurrent first callers pass |
| Task/exec state | Child-only marker clear, inherited restrictions/compat state, readiness/escape denial, failed/successful/repeated exec, absent daemon, disabled SU mode and missing retval pass |
| Legacy early UID negative control | Actual legacy caller changes task flags and schedules work despite a failed LSM UID check; expected defect reproduced |
| Pinned exec negative control | Actual pinned provider ignores escape failure and grants a scoped FD after failed exec while retaining its marker; expected defect reproduced |
| Invalid task configurations | Four expected compile failures: missing built-in KSU, 64-bit storage, THREAD_INFO_IN_TASK or checked policy selection |
| Active automatic SELinux suite | All six positive configurations pass, including checked credential failure/retry in three init paths |
| Historical f443 adapter suite | All seven positive configurations still pass; original defects remain reproducible |
| Exec filename lifetime suite | 27 positive cases pass: nine each manual, SUSFS and hooks-off; baseline ASan UAF reproduced |
| Upstream compatibility checks | Target compatibility fragment passes; all retained invalid-mode/layout checks still reject as expected |
| Separate inline hook checker | Expected make exit 2, now first missing ksu_handle_sys_read |

Host tests use Clang 21.1.8, ASan and UBSan and extract actual changed functions/native masks. Native credentials, seccomp, VFS, task publication, mounts and scheduler behavior are mocked. The fork fixture executes the real shared clearing helper against copied host task state and asserts the actual kernel placement; it does not execute kernel dup_task_struct. No complete ARM64 translation unit, object link, CFI/LTO, lockdep, full policy, boot or device behavior is validated.

Kernel checkpatch returns exit 1 for both diffs: the provider has 189 errors/510 warnings, with every error reflecting BakaSU's native four-space/no-tab indentation. The root diff has 30 errors/25 warnings from the retained exec indentation and the copied upstream header's indentation, function braces, return parentheses and complex-macro formatting. This is not a clean style result; strict source whitespace passes separately. Full reports and final integrity evidence are retained in the checkpoint audit directory.

Initial new fixture failures overgeneralized WebView and allowlist/unmount ordering; they were corrected to match the pinned source. A fixture compile initially omitted two existing provider dependencies. These are host harness development results, not Android kernel compiler results. Logs, extracted source and current source hashes are in .backport-context/audit/setresuid-task-state; earlier passing inputs are preserved in its prior-checkpoint directory.

## Remaining order and preservation

1. Adapt access/stat kernel-filename interfaces, read/init.rc/fstat behavior, reboot transport and input hooks. Review actual guards/prototypes/permissions and finish startup consumers; checker name matches alone are insufficient.
2. Port SUSFS core/header payloads and fsnotify lifecycle; validate native and supported compat command ABI against the pinned userspace tool.
3. Implement mount-ID/minor/group ownership, clone/unwind and namespace semantics together, then filesystem/procfs consumers and optional mapping/redirect features.
4. Replace only this branch's legacy workflow with exact provider/source/config/toolchain pins, retaining every upstream check; run the real ARM64 object/full-build matrix before considering separately approved device gates.

The immutable legacy 1.5.9 patch still applies to a clean baseline. It now deliberately overlaps the new task header/UID integration and must not be applied to this evolving tree. The unchanged legacy workflow would do that and is not a valid build path for this checkpoint. All 125 handoff manifest inputs, source-clean reference repositories, the original checkout and protected devon-resukisu ref remain preserved. No commit, push, CI dispatch, image/release, flash or device operation occurred.
