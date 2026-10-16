# Devon BakaSU automatic SELinux checkpoint

**Checkpoint extension:** The [reboot/input checkpoint](SUSFS_2_3_REBOOT_INPUT.md) now supplies the SUSFS reboot route and accepted input press hook. The unchanged inline name checker passes with manual-guard warnings; earlier missing-input/manual-only findings below describe the prior checkpoint. Core/payload ABI and ARM64 build remain incomplete.


**Extension:** The [2026-10-14 task-state checkpoint](SUSFS_2_3_TASK_STATE.md) extends this provider to 16 files and updates current validation. The nine-file counts and setresuid/legacy applicability results below describe the original 2026-10-13 checkpoint. The subsequent [access/stat checkpoint](SUSFS_2_3_ACCESS_STAT.md) adds owned pathname consumers. Automatic SELinux ownership remains the active route.

Date: 2026-10-13 (Asia/Calcutta). This supersedes the f443 inline-adapter development route. The user authorized assessing and adapting the maintainer's new SUSFS synchronization branch. Source and host validation pass; the complete SUSFS kernel is not integrated or built.

## Selected development inputs

| Input | Exact source / status |
|---|---|
| Motorola kernel | 18bd9752ada79090ce11326662eaaa9f33717728, Linux 4.19.325 |
| BakaSU | e1682a2831142e68fa30bab97d5690cf90b75119, sync-susfs maintainer test branch |
| BakaSU version code | 35229 = 30700 + 4529 history commits; not a release tag |
| Required SID-hash follow-up | d7260ca1e5c128061122924d0d48754e361e135c, incorporated in the local provider patch |
| SUSFS 2.3.0 | 82f08351fadb8b19dd6591bd14634d02ad832145, gki-android12-5.10 |
| External patch | [bakasu-devon-selinux-policy.patch](../.github/patches/bakasu-devon-selinux-policy.patch) |
| Machine-readable selection | [source-pins.json](../scripts/susfs/source-pins.json) |
| Target option | CONFIG_KSU_DEVON_SELINUX_POLICY=y; built-in ARM64 KSU/SUSFS and KALLSYMS_ALL required |

The upstream main branch resolved to d7260ca during the audit and still contained the general SUSFS build stop. The maintainer explicitly requested testing e1682a2 in the original suspension discussion. The new branch legitimately removes that general stop, but keeps a compile-time rejection of SUSFS-owned SELinux hooks below Linux 5.10. Its stated route is to leave non-GKI SELinux interception to BakaSU. No upstream compatibility file or rejection was modified locally, and no downgrade was selected.

The supplied 5.10 SUSFS pin already matches its current upstream branch head. The new BakaSU branch is a test candidate, not certification of this device or the complete pairing. The pinned source-clean reference is preserved separately in .backport-context/repos/bakasu-sync-susfs; the original f443 and SUSFS reference checkouts remain unchanged.

## Source changes and retained behavior

The active Motorola hooks.c and selinuxfs.c are byte-identical to the kernel baseline again. Removed the previous typed inline caller/header from the active implementation. BakaSU's non-GKI context/access/status/setprocattr callbacks now supply the selected SELinux route. SUSFS filesystem hooks remain required separately; choosing automatic SELinux interception does not change the selected SUSFS syscall integration method.

The nine-file provider patch rebases the preceding policy transaction, serialization, init-error and flex-array type-creation work onto e1682a2. A separate, default-off Kconfig option selects the audited Motorola policy path without defining KSU_COMPAT_HAS_SUSFS_FEATURE_SELINUX_HIDE. Kbuild rejects the option for another kernel series, module KSU or non-SUSFS mode; the C layout guard rejects incompatible policy representations. The upstream non-GKI rejection remains effective if the unsupported inline SELinux flag is set.

Motorola already implements context_add_hash and a reverse SID lookup table. e1682a2 and d7260ca are sibling commits, so selecting the test branch alone would miss the hash fix. The patch incorporates d7260ca's compatibility probe and hash-before-insertion behavior for both saved and live contexts. The target still selects live policy/SID-table pointers after taking the native policy read lock and initializes/cleans outputs on every parsed/error path. Real credential transitions use the live SID and retain SETCURRENT, bounded-transition, DYNTRANSITION and ptrace checks.

Automatic context/access/setprocattr readers keep selinux_hide_mutex through the saved-policy operation. Feature changes, transactions and backup retirement share it. Active missing backup fails with EAGAIN. Stock callbacks are captured before publication and retained across unhooking, so a queued callback can return to native behavior after the mutable hook slot/original pointer is cleared. The existing non-target automatic implementations are retained behind the wrappers.

Status opens acquire a page reference and install per-file operations carrying the release callback. The copied operations preserve native read/mmap behavior; the native operations must have no release callback, otherwise installation is rejected. A file keeps its release callback across feature changes, and a mapped VMA keeps its file. Retiring BakaSU's saved-page owner drops only that owner's reference. The target exit path uses the actual selinux_ss status mutex even when late initialization has not populated the generic lock pointer.

The patch does not change SUSFS_VERSION, command definitions, a compatibility check, the legacy patch or workflows. Required/optional rule semantics, real SID caching/credentials and actual text-hook publication still require kernel-level review and validation.

## Preservation of the previous route

The historical provider is retained as [bakasu-devon-selinux-adapter-f443.patch](../.github/patches/bakasu-devon-selinux-adapter-f443.patch). Its kernel header and callers are preserved together in [f443-kernel-selinux.patch](../scripts/susfs/fixtures/f443-kernel-selinux.patch). The historical runner reconstructs both from exact pinned blobs, instead of interpreting current stock callers as the old adapter. All seven historical positive configurations still pass after the migration.

The complete pre-migration diff, exact modified callers/header/provider, previous result and their hashes are also archived under .backport-context/audit/sync-susfs-migration/prior-checkpoint. This evidence is ignored local context, not an applied second implementation. The original handoff and protected branch are unchanged.

## Executed validation

Run the active checkpoint with:

```sh
python3 scripts/susfs/test_selinux_autohooks.py
```

| Check | Actual result |
|---|---|
| Nine-file patch, reverse applicability, reconstructed-source whitespace | Pass, strict application |
| Actual kernel API probes | Pointer SID table, stricter MLS parser and context hash detected; layout selection passes |
| Actual target selection and unmodified SUSFS compatibility fragment | make exit 0; automatic SELinux route selected |
| Wrong kernel series, module KSU, non-SUSFS mode | Three expected make exits 2 |
| Unsupported SUSFS inline SELinux flag on 4.19 | Original upstream compiler error reproduced |
| Remaining unmodified inline-hook check | make exit 2: missing ksu_handle_setresuid |
| Automatic callbacks, static keys off/on | Two positive ASan/UBSan host configurations pass |
| Context and access behavior | Stock fallback, permissions, real credential SID, parser/allocation/insertion/hash failures, cleanup and simulated reload selection pass |
| Status installation mock | Unexpected release rejected; text-patch failure/retry and repeated installation exercised |
| Status/page lifetime | Open/read/mmap/release, owner retirement and feature changes pass under mocks |
| Concurrent reader/backup retirement | Deterministic host synchronization passes |
| Policy lifecycle and init callers | Three positive host configurations pass, including allocation/copy/setup failures and retries |
| Type creation | Actual Motorola flex-array/bitmap/hash algorithms pass 90 allocation failures/retries and nine injected write failures |
| Original upstream status callback negative control | ASan heap-use-after-free reproduced when its page owner is retired |
| Original type-creation negative control | Three count-corruption/type-key-leak cases reproduced |
| Unsupported flex-array layouts | Two expected compiler failures |
| Legacy 1.5.9 patch strict applicability | Pass; still not applied |
| Historical adapter reconstruction | All seven positive configurations pass |

Tests use host Clang 21.1.8 and extract actual source functions. Policy contents, kernel locks, MM/VMA/file reference machinery, symbol discovery and text patching are mocked. No real ARM64 translation unit, complete Kbuild, link, CFI/LTO behavior, kernel hook installation, policy semantics, timing measurements or device runtime is validated by these results.

An initial layout-probe test failed because its mock encoded 4.19.325 without saturation. This vendor Makefile encodes its version-code sublevel as 255. Host fixtures now model that actual generated value; SUSFS and kernel release strings were not changed. The status-install fixture initially omitted the upstream function-pointer typedef; it now extracts that typedef too. These were fixture failures, not kernel compiler results.

Logs, reconstructed snippets, negative sanitizer reports and source/input hashes are in .backport-context/audit/selinux-autohooks. Kernel checkpatch against external BakaSU code exits 1 with 160 errors and 447 warnings. Every error is the tab-indentation rule conflicting with BakaSU's native four-space/no-tab format; warnings include indentation, length/version checks and Kconfig help. This is not a clean style result. Strict source whitespace checks pass independently.

## Remaining implementation order

1. Finish the separate SUSFS inline syscall hooks and their payload/task-state contracts; the unmodified inline-hook check currently stops at setresuid. Review cached SID/credential readiness and remaining policy primitive failures before enabling the target configuration in a real build.
2. Port the SUSFS core/fsnotify lifecycle and required thread/inode flags, then mount-ID allocation and namespace ownership as a coherent series.
3. Integrate filesystem/procfs feature consumers, honest capability reporting and userspace ABI validation.
4. Replace the isolated branch's legacy workflow with the exact source/toolchain/config pins and sole authoritative source implementation. The current workflow still applies 1.5.9 and fetches floating BakaSU; it does not consume source-pins.json and is not a validation path for this checkpoint.
5. Run the actual ARM64 object/full-build matrix through all upstream checks, followed by the separately authorized device validation gates in the port plan. Compilation cannot establish safe boot/runtime behavior.

No commit, push, CI dispatch, image/release, flash or device operation occurred.

Primary sources:

- [Maintainer request to test e1682a2](https://github.com/Baka-SU/BakaSU/commit/48fa4bb7ec8bddb8ea932a8c9be0877f89a1fa73#commitcomment-204751334)
- [SUSFS synchronization commit](https://github.com/Baka-SU/BakaSU/commit/e1682a2831142e68fa30bab97d5690cf90b75119)
- [Retained non-GKI SELinux restriction](https://github.com/Baka-SU/BakaSU/blob/e1682a2831142e68fa30bab97d5690cf90b75119/kernel/feature/selinux_hide.c#L49-L54)
- [SID-table hash fix](https://github.com/Baka-SU/BakaSU/commit/d7260ca1e5c128061122924d0d48754e361e135c)
