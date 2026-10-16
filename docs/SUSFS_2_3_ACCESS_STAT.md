# Devon SUSFS access/stat filename checkpoint

**Extension:** The [read/init.rc checkpoint](SUSFS_2_3_INIT_RC.md) adds held-file FD-stat/read/seek consumers and final-close cleanup. Scope and results below describe the preceding access/stat stage.

Date: 2026-10-14 (Asia/Calcutta). This extends the [task-state checkpoint](SUSFS_2_3_TASK_STATE.md) on the isolated susfs-2.3-backport branch. The user asked to keep working. Source/host checks pass; SUSFS core, other hooks and the integrated ARM64 kernel remain incomplete.

## Scope and exact source

The kernel base remains 18bd9752ada79090ce11326662eaaa9f33717728. BakaSU remains e1682a2831142e68fa30bab97d5690cf90b75119, with the existing d7260ca hash fix and devon policy adaptation. SUSFS remains 82f08351fadb8b19dd6591bd14634d02ad832145. No version or source revision was falsified or compatibility check bypassed.

Changed fs/open.c:do_faccessat, fs/stat.c:vfs_statx and the shared include/linux/ksu_susfs.h. The authoritative [provider patch](../.github/patches/bakasu-devon-selinux-policy.patch) still modifies 16 BakaSU files; this stage extends feature/sucompat.c and .h. The prior passing provider, kernel sources/header, source selection and results are preserved under .backport-context/audit/access-stat/prior-checkpoint. No second active implementation or legacy patch application is used.

## Filename and error contract

SUSFS callbacks take struct filename **. The existing manual callbacks take a userspace filename pointer; broadening the manual guard would pass the wrong type. New SUSFS calls live in the common access/stat lookup helpers, after native argument validation and successful getname_flags. Manual entry hooks, stat copyout/compat code and unrelated source remain byte-identical to the baseline.

The actual Motorola filename_lookup consumes its filename on success and on lookup failure, including internal RCU/ref-walk/stale retries. A hook failure leaves the original reference with the caller, which drops it before returning that error. A successful hook either leaves the original owned reference intact or transfers ownership to a separately allocated kernel filename. Lookup consumes the resulting reference; callers do not put it a second time.

Target SU queries use a common checked provider. It requires credential readiness, no no-SU restriction, enabled SU compatibility, allowlist authorization and the exact /system/bin/su pathname. It checks daemon availability under published KSU credentials and restores the previous subjective credentials before allocating the replacement. Missing readiness, denied authorization, disabled mode or absent daemon leaves the original native lookup intact. Allocation failure returns the actual ENOMEM, leaves the original intact and prevents lookup. A separate getname_kernel(/system/bin/sh) reference replaces the original; the original is released without writing into its name. This preserves any audit-held original and ensures audit reuse sees the original user pathname across outer stale retries.

The pinned access handler deliberately relies on its caller's authorization/no-SU gates. The target adapter owns those checks explicitly, so the common kernel callers cannot omit them. It supports both detected static-key and boolean implementations. BakaSU's actual header imports the shared global filename declaration before its SUSFS prototypes; independent header checks verify the same types.

## Native behavior retained

| Area | Preserved behavior |
|---|---|
| access/faccessat | Both reach do_faccessat; invalid mode and credential allocation keep their native error order |
| Access credentials | Real UID/GID become fsuid/fsgid, native capability/securebits handling and non-RCU override remain; all exits restore/drop the override |
| Access authorization | Vendor inode_permission2, regular-file noexec denial, read-only write denial and special-file behavior remain |
| Pathname stat | vfs_stat, vfs_lstat and vfs_fstatat reach vfs_statx; native/compat syscall wrappers are untouched |
| Stat flags | Symlink-follow, automount, empty-path, query flags and request mask retain native handling |
| Stat security | Actual vfs_getattr invokes security_inode_getattr before metadata; LSM and filesystem errors propagate |
| Stale results | Native inner lookup retry and outer permission/getattr retries remain; filenames and paths balance on every tested path |
| FD stat and copyout ABI | Unchanged in this stage; init.rc fstat handling and full target/compat ABI validation remain separate work |

This does not add faccessat2, a newer statx mount-ID UAPI, real symlink emulation or filesystem hiding. It supplies the existing kernel's filename interfaces and SU compatibility behavior; later SUSFS filesystem consumers must use them coherently.

## Executed source and host validation

Run:

```sh
python3 -B scripts/susfs/test_access_stat.py
python3 -B scripts/susfs/test_setresuid_task_state.py
python3 -B scripts/susfs/test_selinux_autohooks.py
```

The new runner reconstructs/reverse-checks the complete 16-file provider and verifies source whitespace. It extracts actual getname_flags, getname_kernel, putname, filename_lookup, do_faccessat, vfs_statx, vfs_getattr and the three native stat wrappers. Allocation, audit storage, low-level path walk, credentials, permissions and LSM/filesystem operations use explicit host mocks.

Four positive configurations pass under Clang 21.1.8, ASan/UBSan: hooks off, the unchanged manual common path, and target SUSFS with static keys off/on. Tests cover invalid mode/flags, bad/empty/overlong paths, embedded/separate filename allocation failures, missing credentials, redirection allocation failure with/without audit ownership, path errors, inner ECHILD/ESTALE fallback, outer stale retry/audit reuse, real-ID/capability access checks, noexec/read-only/special-file behavior, stat LSM errors, request/lookup flags, empty-path and the native stat wrappers. Target gates and short/nonmatching pathnames preserve the original lookup. All tested references and overrides balance.

Actual provider-header syntax checks pass in both static-key modes. Two intentionally incorrect userspace-pointer callback calls fail compilation with incompatible-pointer errors. A pinned-provider negative control demonstrates that dropping caller gates would redirect an unauthorized/no-SU access query and that its in-place rewrite changes the original audit name. This is evidence for the adaptation, not a claim that the correctly gated upstream 5.10 call site lacks those checks.

The unchanged UID/task and automatic SELinux suites are rerun against the expanded provider/shared header. The unchanged inline checker still fails at the missing read hook, rather than being suppressed. Full results, extracted snippets, compiler/sanitizer output and source hashes are under .backport-context/audit/access-stat. A first harness compile encountered a native signed/unsigned comparison warning under extra host warning flags; the host runner disables that warning for the extracted unchanged filename code while retaining incompatible-pointer warnings as errors. No Android kernel compiler result is claimed.

These tests do not execute real user copies, audit lists, path walk, native credential/security machinery, compat layouts, kernel scheduler behavior, ARM64 translation units, CFI/LTO or device runtime. Full object/link/build and boot gates remain pending. The new kernel-stage diff passes checkpatch with zero errors/warnings. The full external provider exits 1 with 197 indentation errors and 538 warnings; every error conflicts with BakaSU's native four-space/no-tab format. This is not a clean global style result. Final integrity verification passes for all 125 immutable inputs, protected/original source state, three pinned clean repositories, unchanged native lookup/stat wrappers/SELinux callers, recorded test source hashes and 62 local document links. These results are recorded in .backport-context/audit/access-stat/final-integrity.json.

## Remaining work

Next adapt read/init.rc/fstat, reboot command transport and input hooks, then audit remaining startup/credential consumers. Port the SUSFS core, command payload ABI and fsnotify lifecycle before mount-ID/namespace ownership and filesystem/procfs consumers. Replace the isolated branch's legacy workflow with pinned source/config/toolchain inputs before actual ARM64 builds. Device/image operations remain outside this stage.

The original checkout, devon-resukisu ref, all 125 immutable handoff inputs, clean pinned reference repositories, legacy patch and workflow remain preserved. No commit, push, CI dispatch, release, flash or device/image operation occurred.
