# SUSFS 2.3.0 backport log

Entries use the user's Asia/Calcutta date context. Record actual results and distinguish source review, applicability, compilation, boot and runtime validation.

## 2026-10-13 — Documentation and evidence checkpoint

**Scope:** The user approved the first proposed documentation/provenance change after the initial read-only assessment.

**Source pins:**

- Kernel: 18bd9752ada79090ce11326662eaaa9f33717728.
- Protected devon-resukisu ref: 80f0ee4d1ec68838d25c2efeb955d31e90f85dae.
- SUSFS 1.5.9: 1f764cfb8ddc3b19d7624731051d1000d629175b.
- SUSFS 2.3.0: 82f08351fadb8b19dd6591bd14634d02ad832145.
- BakaSU: f4435f007b16f61705089ff982e3f981a6017979.

**Workspace:** The app worktree tool rejected its arguments. Native WSL git worktree add -b susfs-2.3-backport created /home/shinji/projects/android_kernel_motorola_sm6225-susfs-2.3-backport from the pinned kernel commit; exit 0. The original checkout and protected branch were not switched or edited.

**Changes:** Copied BACKPORT_HISTORY.md byte-for-byte, preserved .backport-context references locally, and prepared provenance, API matrix, port plan and SHA-256 input manifest. Kernel source, legacy patch, workflows and BakaSU checks remain unchanged. Local evidence/audit logs remain ignored; they are not proposed kernel changes.

**Executed checks:**

| Check | Result |
|---|---|
| Legacy git apply --check --verbose --whitespace=error-all | Exit 0 |
| 5.10 git apply --check --verbose | Exit 1, expected; 19/24 paths fail, including missing bootconfig |
| Baseline git diff --check | Exit 0 |
| ZIP/extracted input comparison | All 120 entries match |
| Legacy patch reconstruction in memory | All 15 Motorola source snapshots match |
| Pinned upstream Git blob verification | All 99 BakaSU and five SUSFS files match |
| Evidence manifest | 125 input files recorded by path, bytes and SHA-256 |

Logs and machine-readable comparison records are under .backport-context/audit/. The new susfs-23-compatibility.log is an audit result, not a recovered historical Codespace log.

**Findings:** The previous supplied Motorola changes are preserved in the committed patch. BakaSU's unconditional compatibility stop remains. Critical work includes filename types/lifetime, inactive stat/reboot hooks, init.rc stat integration, fsnotify callback and mark cleanup, shared mount-ID ownership, private flags, statx policy, and consistent filesystem/procfs behavior. A compile result would not prove boot/runtime safety.

**Validation status:**

- Evidence identity and patch applicability: executed, with results above.
- Source/interface audit: documented; no implemented port reviewed as complete.
- SUSFS compile/link: not performed.
- Image/repack/release: not performed.
- Temporary boot/device operations: not performed.
- Runtime validation: not performed.
- Push/CI dispatch: not performed.

**Documentation verification:** All local document links, 72 preserved-snapshot file/line references and all 125 input hashes were checked. The review diff contains exactly the copied handoff and five new documentation/manifest files; no source, patch or workflow changes. Git intent-to-add entries make the documentation visible in the unstaged diff; nothing is staged for commit.

The default full git diff --check reports two inherited trailing-space lines in BACKPORT_HISTORY.md (lines 3 and 4). These are the original Markdown hard line breaks, retained to preserve the input byte-for-byte. New docs pass the default whitespace check. The handoff passes a check scoped to that file with blank-at-eol disabled; this exception does not apply to source files or new docs.

Final commands: git diff --check -- docs (exit 0), and git -c core.whitespace=-blank-at-eol diff --check -- BACKPORT_HISTORY.md (exit 0). Original checkout HEAD, branch, status and handoff bytes, plus the protected ref, were rechecked unchanged. Machine-readable results are retained at .backport-context/audit/documentation-validation.json.

## 2026-10-13 — BakaSU compatibility audit and first kernel correction

The user authorized starting implementation. Full source-clean BakaSU and SUSFS Git checkouts were obtained at the existing pins under .backport-context/repos/. All 99 BakaSU and five SUSFS supplied files match the full checkouts. BakaSU version code 35227 was reproduced from the Git history count.

The guard originated at 48fa4bb7ec8bddb8ea932a8c9be0877f89a1fa73. Subsequent SELinux change fa12ffde8244008e9afdb7ac0c1bb1bc1b06b95a removes fake_state and preserves paired live/backup-policy context validation. SUSFS 2.3.0's integration patch reintroduces fake_state and removes original SID/error outputs and the paired hook validation call. The supplied KernelSU patch fails against BakaSU in 24 files (exit 1). No older revision was selected; the actual rc3 tag resolves to 239e1e8871b8fcd51a6e5b3002e0ba522fdd99fb, not the failed-build source commit.

The unmodified BakaSU compatibility fragment was evaluated using host make, with an inert target supplied after its include. It exits 2 at the existing intentional stop. The guard remains byte-identical to the supplied snapshot. Integrated build support remains unresolved; no full build was attempted.

Changed fs/exec.c to retain a valid filename through the consuming exec implementation and post-exec hook, then release the retained reference. The hook's mutable pointer argument does not own that extra reference. No SUSFS configuration guard was broadened. This independent correction is needed by the later integration, so it can be reviewed before resolving the full build gate.

Added scripts/susfs/test_exec_filename_lifetime.py. It extracts the actual source wrapper and a pinned Git baseline, compiles host mocks with ASan/UBSan and checks callback access, return codes, replacement/error paths and balanced references. The old baseline reproduces a heap-use-after-free. All nine manual-hook cases and nine hook-disabled cases pass for the corrected source (runner exit 0).

Host tooling was initially absent. Installed Ubuntu WSL clang, libclang-rt-dev, make and libc6-dev through apt (update/install exit 0); dependencies included libc updates. Tests used host Clang 21.1.8, not the Android ARM64 kernel toolchain. These are host ownership tests, not a full kernel build or device proof.

Negative result: placing the new variable declaration inside the old patch's guard context initially broke legacy patch applicability (exit 1, fs/exec.c). A nested declaration scope preserves that context and C declaration ordering. After the adjustment, the strict legacy patch check passes (exit 0), and the ownership tests were repeated successfully.

Logs: .backport-context/audit/bakasu-pairing-results.json, bakasu-guard-check.log, bakasu-reference-apply.log, legacy-patch-after-exec-fix.log and exec-lifetime/results.json. The [compatibility audit](SUSFS_2_3_BAKASU_COMPATIBILITY.md) records the exact forward-integration requirements and limitations.

Source/interface audit and host regression: executed as described. ARM64 kernel compile/link, images, boot and runtime: not performed. No push, CI dispatch, repack/release, flashing or device operation. Protected branch and original checkout remain unchanged.

## 2026-10-13 — SELinux interface adaptation

The user explicitly requested starting the SELinux adapters. Added the typed kernel header and guarded Motorola callers for current-context validation, context canonicalization, access-vector queries and referenced status-page access. Credential changes keep the live SID and Motorola permission/transition/ptrace checks. No fake_state or version spoofing is introduced.

Prepared .github/patches/bakasu-devon-selinux-adapter.patch against f4435f007b16f61705089ff982e3f981a6017979. It changes only kernel/Kbuild, kernel/feature/selinux_hide.c, kernel/feature/selinux_hide.h and kernel/selinux/rules.c in a reconstructed BakaSU tree. It synchronizes saved-policy readers/publication/destruction, selects live pointers after the policy read lock, fixes paired-parser output/cleanup paths, and selects one typed inline SELinux path. Reference source checkouts are unchanged. The workflow and legacy device patch were not changed.

The original susfs_compat.mk remains SHA-256 25eb93fe03d155f5cb19f1de8a4e291d88f0016ff0f7e53b558f63fae1a9e2a7. Evaluating the new interface probe and then that fragment detects the adapter flags and exits 2 at the original unconditional stop. No supported complete pairing is claimed.

Added an executable host runner and C fixture. Strict BakaSU patch application, reverse applicability and reconstructed-source whitespace pass. Actual source functions compile in three host configurations (SUSFS off, on without static keys, on with static keys) under ASan/UBSan, and all behavioral/lifetime checks pass; runner exit 0. Incomplete/module header linkage fails as intended. Checkpatch reports 0 errors/1 generic MAINTAINERS warning. Fixture-development failures and precise test limits are recorded in the [adapter checkpoint](SUSFS_2_3_SELINUX_ADAPTER.md).

Validation logs and input/patched-source hashes: .backport-context/audit/selinux-adapter/. Strict legacy patch applicability still passes. Protected origin/devon-resukisu remains 80f0ee4d1ec68838d25c2efeb955d31e90f85dae and the original checkout still has only its untracked handoff.

This checkpoint validates source and host contracts. Actual ARM64 build, policy/lockdep/RCU/MM behavior, images, boot and device runtime remain unvalidated. No commit, push, CI dispatch, release, flash or device operation.

## 2026-10-13 — SELinux policy lifecycle and failure-path checkpoint

The user approved starting the lifecycle review. Traced native policy loading/initialization/reload/disable and BakaSU setup, updates, backup drop, constructors and all initialization callers. Found unchecked policy allocations, premature/repeated backup replacement and an early copy-error path that unlocks an unacquired mutex in the original legacy handler.

Extended the existing pinned provider patch to seven BakaSU files. The target path now prepares/publishes private policy transactions, retains the first saved policy and never recreates it from a modified policy after retirement, checks serialization capacity arithmetic and propagates copy/setup errors through all three built-in init-exec paths before SID/credential caching. Other modes retain the original void API/implementation. Default-rule statements were extracted byte-identically; primitive mutation semantics remain unchanged.

Expanded scripts/susfs/test_selinux_adapter.py and added scripts/susfs/test_selinux_policy_lifecycle.c. All six ASan/UBSan host configurations pass (runner exit 0), covering three prior adapter modes and three independent init paths with eight preparation allocation failures, four update-copy allocation failures, constructor/serializer errors, readiness, snapshot retention/retirement, malformed-batch rollback, partial-success counts, size limits and retry/completion gating. Strict complete-patch/reverse/source-whitespace checks pass. BakaSU's unchanged compatibility stop still returns make exit 2. The external patch's kernel checkpatch result is not clean: 101 indentation errors/306 warnings; all errors conflict with BakaSU's explicit four-space/no-tab format. Details and limitations are in the [lifecycle checkpoint](SUSFS_2_3_SELINUX_LIFECYCLE.md).

A further concrete risk was identified in add_type's allocation and partial-mutation behavior. It remains a required source/fault-test checkpoint before policy installation can be called safe. Real policy semantics, SID-cache readiness, kernel concurrency/lockdep/RCU/CFI and device behavior remain unvalidated. No compatibility bypass, version spoofing, commit/push, CI/image/boot/flash operation. Protected branch and original checkout remain unchanged.

## 2026-10-13 — SELinux type-creation allocation checkpoint

The user authorized the next step. Verified and archived the preceding passing lifecycle result/provider patch before editing. Implemented the audited devon flex-array add_type path with private preparation, checked copies/bitmap changes, last-fallible hash insertion, ownership transfer and failure cleanup. Required KSU domain/file-type and permissive setup failures now abort the private transaction before SID/credential caching. Other modes retain their original type-creation branch; their default rule statements match the pinned source after preprocessing.

Added scripts/susfs/test_selinux_type_creation.c and extended the existing runner to compile actual Motorola flex-array/reciprocal/bitmap/hash algorithms. All seven positive ASan/UBSan configurations pass (runner exit 0): the prior six plus 90 type-creation allocation failures with successful retries, nine injected flex-write errors, boundary/layout/idempotency tests and balanced ownership. Three compiled baseline negative cases reproduce count corruption and datum/key leaks. Two unsupported type-layout compiler failures and the existing module/incomplete-interface rejection are expected results. Strict patch/reverse/reconstructed-source whitespace pass; the unmodified upstream compatibility guard still exits make 2. External kernel checkpatch remains a native-format conflict (156 tab-indentation errors, 429 warnings), not a clean style check.

The [type-creation checkpoint](SUSFS_2_3_SELINUX_TYPE_CREATION.md) records the implementation, actual native algorithms used, host substitutions, negative results and limitations. Broader attribute/constraint/optional-rule errors, real SID-cache/credential readiness, policy semantics and ARM64/kernel/runtime validation remain gates. No guard bypass, version spoofing, protected-branch edit, commit/push, CI/image/boot/flash operation.

## 2026-10-13 — Upstream synchronization and automatic SELinux route

The user authorized assessing and adapting the maintainer's new SUSFS synchronization branch. Pinned e1682a2831142e68fa30bab97d5690cf90b75119 from sync-susfs as the development source, without downgrading. Its upstream commit removes the general SUSFS stop while retaining an explicit rejection of SUSFS-owned SELinux hooks below 5.10. On 4.19 the upstream route lets BakaSU own that interception. Main still resolved to d7260ca1e5c128061122924d0d48754e361e135c with the general stop during this audit. The immutable SUSFS 5.10 pin remains its branch head.

Preserved the complete preceding diff, exact header/callers/provider, previous results and hashes under .backport-context/audit/sync-susfs-migration/prior-checkpoint. Archived the old provider as bakasu-devon-selinux-adapter-f443.patch and its kernel interface/callers as scripts/susfs/fixtures/f443-kernel-selinux.patch. Restored only our two inline SELinux caller edits to the kernel baseline and removed the unused active header. The historical runner now reconstructs the preserved callers and provider; its seven positive configurations still pass. The exec ownership fix remains active.

Prepared the authoritative nine-file bakasu-devon-selinux-policy.patch against e1682a2. Rebased the policy transaction, serializer, init-error and flex-array allocation work under a separate default-off target option. Preserved both upstream restrictions/check files; the new target does not set the unsupported inline SELinux flag. Incorporated d7260ca's required context_add_hash probe/insertion fix, because Motorola has reverse SID hashing and that fix is absent from the sibling test branch. Added protected automatic readers, persistent stock callback fallbacks and referenced status pages with per-file release operations.

The active host runner strictly reconstructs/reverse-checks the nine-file patch and verifies source whitespace. The actual target selection plus unmodified SUSFS compatibility fragment passes with make exit 0. Three invalid selections fail with exit 2, the unsupported inline SELinux flag still triggers the upstream C error, and the separate unmodified inline check still exits 2 at missing ksu_handle_setresuid. All six positive ASan/UBSan configurations pass: two automatic-hook modes, three policy-init paths and type creation with 90 allocation failures/retries and nine write failures. Baseline negative controls reproduce status-page UAF and type-count/key ownership defects; two incompatible type layouts fail compilation as intended.

Fixture development initially misencoded the kernel version-code sublevel; the vendor Makefile always uses 255, which the fixtures now model. A subsequently added status-install mock lacked its function-pointer typedef; the runner now extracts it from actual source. Neither was a real kernel build failure. Native-format kernel checkpatch remains a style conflict, not a clean result; detailed classification is in the final validation evidence.

See the [automatic-hook checkpoint](SUSFS_2_3_SELINUX_AUTOHOOKS.md) and scripts/susfs/source-pins.json for the current source selection. Logs and source hashes are under .backport-context/audit/selinux-autohooks; earlier checkpoints remain historical evidence. The legacy patch/workflow and handoff are unchanged. Real ARM64 compile/link, kernel text patching, policy semantics, timing/lockdep/MM behavior, images and device validation remain unperformed. No commit, push, CI dispatch, release, flash or device operation.

## 2026-10-14 — validated UID/task-state and credential-publication checkpoint

The user authorized beginning the remaining implementation stages while the upstream PR remains open. Retained BakaSU e1682a2831142e68fa30bab97d5690cf90b75119 and SUSFS 82f08351fadb8b19dd6591bd14634d02ad832145. No upstream revision, version or compatibility check was changed. Preserved the preceding passing provider, affected sources/fixtures/results and full diff under .backport-context/audit/setresuid-task-state/prior-checkpoint.

Added the unchanged upstream 2.3.0 definitions with standalone includes/storage guards and a shared target UID/readiness/fork contract. Moved target UID side effects after every native fallible UID/LSM check and before the vendor's infallible commit_creds. Restricted them to real equal-UID transitions in init_user_ns. Fork clears only the pending exec marker; inherited SUSFS, root-escape and native compat flags remain. Enabled the filename ownership fix for SUSFS mode. Extended the authoritative provider patch from nine to 16 files with checked and serialized SELinux credential publication, startup failure propagation/retry, target UID readiness and Zygote-next no-SU clearing, and one-shot SU exec state. SU exec rejects missing readiness/root-escape restriction, propagates escape failures, and installs a scoped FD only following successful exec. No core/header payload handler, mount or other filesystem consumer is represented as complete.

The new actual-source UID/task fixture passes three positive configurations with Clang 21.1.8, ASan/UBSan: KSU off and target SUSFS static keys off/on. Four invalid configurations fail compilation as expected. Two actual-source negative controls reproduce legacy early UID side effects despite LSM denial, and pinned exec granting a scoped FD after failed escape/exec while leaving its marker set. Failed/partial/no-op/userns/uid16 paths, Zygote classes/ordering, readiness failures/retries and eight concurrent setup callers, inherited child flags and exec errors/duplicates are covered under explicit host mocks.

Fixture development initially omitted existing dependencies and overgeneralized WebView classification and Zygote-next allowlist/unmount ordering; corrected the tests to the pinned behavior. These were harness failures, not actual ARM64 build results. The preserved upstream escape helper returns zero for an inherited root-escape restriction, so the new SU pre-hook checks that restriction explicitly without bypassing it.

Re-ran all six automatic SELinux configurations, all seven historical f443 configurations and 27 positive exec-lifetime cases (manual/SUSFS/off); all passed with original sanitizer/type negative controls retained. Strict reconstruction/reverse application of the 16-file provider and source whitespace pass. The unmodified inline check still fails with make exit 2, now at ksu_handle_sys_read. Its access-hook name match is insufficient: fs/open.c still contains the old manual-only user-filename interface.

Final audit verifies all 125 immutable inputs, the original checkout/status, protected ref, three clean pinned source repositories, exact provider regeneration, stock native SELinux callers, all current recorded test input hashes and 55 local document links. The immutable legacy patch passes strict applicability against all 23 paths on a clean base; application to this evolving tree intentionally fails. Checkpatch exits 1: external provider 189 indentation errors/510 warnings; root diff 30 style errors/25 warnings, with copied upstream header/retained exec formatting. Neither is represented as a clean checkpatch result. Source whitespace passes, apart from deliberately preserved original handoff Markdown breaks and raw patch payload handling.

See [SUSFS_2_3_TASK_STATE.md](SUSFS_2_3_TASK_STATE.md) for the exact contract, tests, limits and remaining order. Current logs/hashes are in .backport-context/audit/setresuid-task-state, selinux-autohooks, selinux-adapter-f443 and exec-lifetime. The immutable legacy patch is validated against the clean baseline and now intentionally overlaps this evolving tree; its unchanged workflow is not a build route for the new integration. Original handoff/manifest inputs, source references and protected branch are preserved. Real ARM64 object/full builds, kernel policy/seccomp/VFS/locking behavior and boot/runtime tests remain unperformed. No commit, push, CI dispatch, image/release, flash or device operation.

## 2026-10-14 — access/stat owned filename checkpoint

The user asked to keep working. Preserved the previous task-state source/provider/header/results under .backport-context/audit/access-stat/prior-checkpoint. Retained every pinned revision and the 16-file authoritative provider scope. Added SUSFS-only kernel filename calls in do_faccessat and vfs_statx and shared typed declarations. Both access syscalls and all existing pathname stat helpers now reach those calls. Existing manual entry hooks, unrelated source, compat/copyout and FD-stat code are unchanged.

The target provider checks readiness, no-SU restriction, SU mode, allowlist membership, exact SU name and daemon availability. It restores temporary KSU credentials before allocating a separate shell filename, transfers one owned reference and preserves the original audit pathname. Allocation errors leave the original with the caller; callers drop it and propagate the actual error. The native lookup consumes names on all lookup exits. Existing access real-ID/capability logic, inode_permission2, noexec/read-only checks, stat LSM/request flags and retries remain.

Four actual-source host configurations pass with Clang 21.1.8 and ASan/UBSan: hooks off, manual common path and SUSFS static keys off/on. Actual filename allocation/putname/lookup and stat wrappers are extracted. Tested mapping covers filename/credential/allocation errors, embedded/separate names, audit retention/reuse, redirection failures, permission/LSM checks, inner/outer stale retries, lookup/query flags and all provider gates. Two wrong userspace-pointer calls fail compilation; both actual provider-header selections pass. The pinned-provider negative control demonstrates why its caller gates are necessary and reproduces original audit-name mutation under in-place redirect. It does not assert that the upstream 5.10 caller omits those gates.

The initial edit script stopped at an exact suffix assertion before writing the provider; root edits were preserved and the provider edit was then completed with bounded source regions. A first harness build reported a signed/unsigned comparison in unchanged native code under extra host warnings; only that host warning was disabled, with pointer compatibility still enforced. These are development-tool/harness results, not real ARM64 kernel build failures.

See [SUSFS_2_3_ACCESS_STAT.md](SUSFS_2_3_ACCESS_STAT.md) for contracts, executed validation, limits and remaining order. Real audit/VFS/user-copy/credential behavior, compat layouts, full ARM64 Kbuild/link, CFI/LTO and boot/runtime remain pending. The next integration remains read/init.rc/fstat, reboot and input, followed by core/fsnotify/ABI, mount ownership and filesystem consumers. No upstream check/version was bypassed; no commit, push, CI dispatch, image, release, flash or device operation.

Final checkpoint validation: all four new access/stat configurations, three UID/task configurations and six automatic SELinux configurations pass; their actual-source negative/error controls remain intact. The provider strictly reconstructs all 16 files and reverses cleanly. The unmodified inline checker still exits 2 at missing ksu_handle_sys_read. The unchanged exec source hash matches its passing 27-case result, and the unchanged historical f443 inputs match their passing results. The new 92-line kernel-stage diff has zero checkpatch errors/warnings; the full external provider has 197 tab-indentation errors/538 warnings, retained and classified rather than represented as clean. Verified all 125 immutable inputs, original checkout/status, protected ref, three clean source pins, eight preserved previous inputs, current test source hashes and 62 local document links. Native filename lookup/stat wrappers/SELinux callers remain byte-identical to the base. Evidence is in .backport-context/audit/access-stat/final-integrity.json.

## 2026-10-14 — synchronous init.rc FD/read checkpoint

The user authorized starting the next stage. Preserved the access/stat source/provider/header/pins/results under .backport-context/audit/init-rc/prior-checkpoint. Kept every source pin and the same 16-file external provider scope. Added held-file FD-stat/read/iterator/seek APIs and final-close cleanup, with no native structure or file-operation/private-data replacement. The target uses per-file sidecars and an immutable bounded boot tail; the original non-target proxy remains guarded.

Native mode/user-buffer/read-LSM/getattr validation precedes the hooks; FD size is changed only for a successful valid STATX_SIZE result. Exact PID/UID/init-context/readiness/path/open-mode gates select the overlay. Prefix reads remain native, tail copies use logical positions, partial faults advance only delivered bytes, and repeated fstat/EOF/rewind are consistent for the supported FD view. Module data is read completely within a 1 MiB bound with retry/unwind; absent/disabled custom data selects the unchanged static RC. Cache I/O and view/final-close synchronization are separate, and native file references govern borrowed sidecar lifetime. Cache bytes are intentionally retained until reboot; close frees the sidecar before object reuse.

The staged overlay supports synchronous scalar/vector/positioned reads and logical SEEK_END; async/flagged iterator requests and SEEK_DATA/HOLE are explicitly unsupported for eligible RC views. Physical pathname stat, mmap/splice and other inode interfaces remain outside this FD overlay. Actual init/ROM consumer behavior, target/compat compilation and kernel lifetime/locking must be validated before boot.

Four new actual-source host configurations pass under Clang 21.1.8, ASan/UBSan. Tests cover byte/size/position consistency, native errors/valid-size masks, partial copies, iterator counts/scatter IO, independent opens, close/reuse, module errors/short reads/bounds, allocation retry, extent changes, process/credential gates and concurrent first publication. The actual pinned negative control reproduces false EOF, unchanged append position, shrinking module size and spoofed-comm FD-stat behavior. Initial harness failures involved the close-marker substring and GNU/Linux mock type/iterator declarations; these were corrected without bypassing checks or altering kernel versions.

See [SUSFS_2_3_INIT_RC.md](SUSFS_2_3_INIT_RC.md) for exact boundaries and evidence. Source/host results do not establish ARM64 build, CFI/LTO, native file/LSM behavior, lockdep or safe boot. Remaining work begins with reboot/input, then startup/core/fsnotify/command ABI, mount ownership and filesystem consumers. No commit, push, CI dispatch, image, release, flash or device operation.

Source review also preserves the upstream init.rc enable flag for new views, maintains existing view contracts after disable/rename, requires a live opened non-stream file, and uses nonblocking module opens with absent/non-directory fallback. Final inspection corrected an earlier progress statement: the unmodified checker now stops at input, while reboot is merely found by name under its old manual-only guard. Reboot remains unadapted for SUSFS. All 17 current positive source/host configurations pass (four init.rc, four access/stat, three UID/task, six automatic SELinux), with their error and actual-source negative controls retained.

Final preservation/style audit passes: all 125 immutable inputs, original checkout/status and protected ref, three clean pins, nine archived prior inputs, recorded result hashes and 67 local links. Native structure/lookup/SELinux files and unchanged exec ownership hash remain preserved. The new kernel-stage diff passes checkpatch (zero errors/warnings); full external provider exits 1 with 293 tab-indentation errors/790 warnings, recorded as style findings. Strict provider reconstruction/reversal and source whitespace pass. Final evidence is .backport-context/audit/init-rc/final-integrity.json. The unchanged checker exits 2 at input; reboot's manual-only name match remains insufficient.

## 2026-10-14 — reboot transport and input checkpoint

The user authorized the next checkpoint. Preserved ten previous source/header/provider/pins/test/result inputs under .backport-context/audit/reboot-input/prior-checkpoint. The pinned sources and 16-file provider scope remain unchanged. Added the SUSFS reboot guard and typed shared contract; the native reboot flow and pinned provider are preserved, including payload-buffer replies and native EPERM/EINVAL transport returns.

Input now observes copied accepted volume-down value-1 edges, with native filtering, delivery and its single entropy contribution unchanged. The target provider counts under an IRQ-safe spinlock, saturates at three and freezes the count before safe-mode sampling. Concurrent post-fs-data/query callers publish the cutoff under the same lock and only one disables the static key in process context after releasing the lock. No sleep/work/static-key patching occurs in the input callback. Existing non-target provider routes are retained.

Eight new host configurations pass under Clang 21.1.8, ASan/UBSan, covering actual extracted reboot/input functions with explicit namespace/driver/extension/action and IRQ/lock mocks. Two invalid prototype controls fail compilation; the pinned input negative control reproduces repeat-driven false safe-mode detection. Initial fixture expectations incorrectly treated the returning restart2 mock as zero and command zero as unknown; actual native code/constants corrected those test expectations without changing kernel behavior.

The unchanged upstream inline checker now exits zero with every required name found and manual-guard warnings retained. The automatic SELinux runner requires this genuine result, while retaining the compatibility fragment, non-GKI rejection and invalid-mode failures. See [SUSFS_2_3_REBOOT_INPUT.md](SUSFS_2_3_REBOOT_INPUT.md) for contracts and limits. Full SUSFS payload dispatch/FD permission scope, native input IRQ/jump-label behavior, core/fsnotify/mounts, ARM64 Kbuild/link, CFI/LTO and boot safety remain unvalidated. No commit, push, CI dispatch, kernel image, release, flash or device operation.

Final checkpoint audit passes all 125 immutable inputs, the original checkout/status and protected ref, three clean source pins, ten archived prior inputs, current/historical recorded input hashes, unchanged exec ownership results and 81 local document links. All 25 current positive host configurations pass (eight reboot/input, four init.rc, four access/stat, three UID/task and six automatic SELinux); their error and negative controls remain enforced. Native filesystem structure/lookup and SELinux files remain byte-identical to the base. The new 69-line kernel-stage patch has zero checkpatch errors/warnings. The full external provider has 299 tab-indentation errors and 817 warnings (exit 1), retained as style findings rather than represented as clean. Strict provider reconstruction/reversal and reconstructed-source whitespace pass. The regenerated authoritative provider SHA256 is 0ffacef0234e2b66b80a59cc333c88b36aeba547b475d4542f826b27d9fee6da. Evidence is .backport-context/audit/reboot-input/final-integrity.json.

## 2026-10-14 — SUSFS core foundation

The user authorized starting core work. Preserved nine previous inputs/results and recorded that fs/susfs.c/include/linux/susfs.h were absent. Imported the byte-identical 2.3.0 header and implemented the staged core/Makefile registration, query/log handlers and 4.19 fsnotify monitor. The 16-file pinned BakaSU provider is unchanged; its existing init and boot-completed consumers call these entry points.

The worker uses readiness-published KSU credentials with override/revert, retries errors/missed events, validates Android directory resolution and only then disables the not-decrypted key. Callbacks request work without lookup, mutexes, group destruction or static-key patching inside SRCU. Parent-path/group/creator-mark references are owned explicitly; failures publish nothing and cleanup drops the creator mark before group destruction/attachment removal, then the path. A required vendor free_mark callback is present. Extra work is initialized statically before any inline consumer.

Only optional ENABLE_LOG is implemented as a configurable feature. Eight unported core/consumer flags cause compile errors. The actual AVC payload receives EOPNOTSUPP while its consumer is absent; capabilities are not falsely advertised. Query outputs are completely initialized and retain the pinned layouts/constants/version. Version output does not establish full port completeness. The legacy Kconfig defaults/workflow are not an integrated validation path.

Four whole-core configurations pass under Clang 21.1.8, ASan/UBSan, with actual transport/dispatch and extracted vendor lifetime primitives under explicit host VFS/fsnotify/workqueue/copy models. Negative controls reproduce feature-name echoing and the retained creator mark/group; the original modern ops fail the actual 4.19 table interface. Eight unported flags and absent FSNOTIFY fail compilation. ARM64/ARM32 freestanding syntax probes validate five fixed-layout core payloads against exact JNI declarations; Rust repr(C) fields are inspected. Full feature/native-long/compat transport ABI remains pending. Initial harness repairs addressed the upstream inline-brace extractor, GNU/Linux mock UAPI/errno includes, model logging/static-key declarations and an unused-variable warning confined to the unchanged negative control.

See [SUSFS_2_3_CORE_FOUNDATION.md](SUSFS_2_3_CORE_FOUNDATION.md) for scope and evidence. Feature bodies, path/SRCU and mount ownership, filesystem/procfs/AVC consumers, real ARM64 object/full-kernel compilation and device gates remain incomplete. No compatibility check or version bypass, commit, push, CI dispatch, kernel image, release, flash or device operation.

Final core checkpoint audit passes all 125 immutable inputs, protected ref/original checkout, three clean exact source pins, nine preserved inputs and recorded absent files, current/historical result hashes, unchanged exec ownership results and 92 local document links. All 29 current positive host configurations pass (four core plus the previous 25); both target syntax probes pass for five supported payload layouts. The byte-identical imported header and unmodified native filesystem/SELinux/fsnotify files are verified. The authoritative BakaSU patch remains 0ffacef0234e2b66b80a59cc333c88b36aeba547b475d4542f826b27d9fee6da. Source whitespace and strict provider reconstruction/reversal pass. The core/Makefile patch has zero checkpatch errors and one new-file MAINTAINERS warning; the unchanged imported header has three spacing errors and five warnings. These findings are retained, with no global clean-style claim. Evidence is .backport-context/audit/core-foundation/final-integrity.json.

## 2026-10-14 — path-core portion of feature integration

The user authorized the next major stage. Preserved eight prior source/test/pin/result inputs and recorded the absence of fs/susfs_path.c. Added the real isolated path-core object, conditional Makefile selection and typed declaration-only header extension. Removing that exact extension reproduces the pinned header; all versions/constants/structures remain unchanged. The external BakaSU provider and upstream checks remain unchanged.

Path commands copy/validate inputs, preserve all 255 valid bytes, propagate real lookup errors, balance held paths and atomically mark only bit 33. Immediate marking uses caller credentials/namespace/CWD; deferred names require absolute initial-namespace/full-root context, with EXDEV for private views. Loop registration deduplicates before allocation, caps unique entries at 1024, and publishes immutable entries under a writer mutex with vendor RCU publication. SRCU readers name their domain explicitly and never hold the writer mutex across VFS IO. Readiness, non-null credentials and worker context gate each pass; overrides/reverts and path references are balanced. Predicate semantics preserve umounted-app and inode-owner exceptions, using the same public mapping for regular/FUSE inodes.

The production aggregate still rejects SUS_PATH and its capability query does not advertise it: lookup/dcache and readdir consumers are pending. This is a real isolated subsystem source/host checkpoint within the first remaining major stage, not a complete feature or full kernel build. The existing whole-core runner still exercises that aggregate rejection.

Two path-object configurations pass under Clang 21.1.8, ASan/UBSan, with actual transport/dispatch and core work consumer under explicit host VFS/SRCU/credential/workqueue models. Tests cover input/copy/permission/error/refcount paths, maximum names, allocation failures, namespace/root/task predicates, capacity/duplicates, blocked-reader/concurrent-writer publication and retries. A pinned-function negative control reproduces a 255-byte name silently shortened to 254. Bad legacy payload/predicate prototypes and absent SRCU fail compilation. Two target syntax probes verify the path payload's 260-byte size/256 err offset against exact JNI definitions; Rust repr(C) fields are inspected. Initial harness repairs imported missing native list helpers and the negative-control log macro. No upstream compatibility check was bypassed.

See [SUSFS_2_3_PATH_CORE.md](SUSFS_2_3_PATH_CORE.md). Next work is consumer integration before production enablement, followed by remaining feature/mount/build gates. Real kernel SRCU/workqueue/namespace/FUSE/permissions, all native/compat consumers, ARM64 objects/full kernel and device safety remain unvalidated. No commit, push, CI dispatch, image, release, flash or device operation.

Error-field replies now check unsigned address addition before copy-out, including the shared core error helper. Near-maximum malformed user pointers are exercised with the actual vendor overflow helper/macro, avoiding wrapped field addresses and pointer-overflow sanitizer faults. Payload conventions and production feature gates remain unchanged.

Final path checkpoint audit passes all 125 immutable inputs, protected ref/original checkout, three clean pins, eight preserved inputs and the recorded absence of the new source, current/historical input hashes, unchanged exec ownership results and 103 local document links. There are 31 current passing host configurations: the two path, four core and six automatic SELinux configurations were exercised for this checkpoint; other source/test inputs match their passing recorded results. Core/path undefined-behavior sanitizer reports now halt instead of recovering. Both target probe pairs pass for six fixed-layout payloads. Seventeen inspected native consumer/structure/SELinux/fsnotify/helper files remain unchanged from the base. The external 16-file provider remains 0ffacef0234e2b66b80a59cc333c88b36aeba547b475d4542f826b27d9fee6da and reconstructs exactly. Source whitespace passes. The new 237-line source/header/object-selection stage diff has zero checkpatch errors and one new-file MAINTAINERS warning; prior imported-header/provider style findings remain recorded, with no global clean-style claim. The unchanged inline checker finds all seven hooks and retains manual-guard warnings. Evidence is .backport-context/audit/path-core/final-integrity.json. Production SUS_PATH rejection/capability exclusion remains intentional until the consumers are complete.

## 2026-10-14 — initial path lookup and directory consumers

The user authorized the next planner step. Preserved 14 previous inputs and the full preceding diff. Added guarded namei lookup/cache/slow/open/walk consumers and all five native/compat directory emitters. Referenced hidden lookups return ENOENT without fake-name substitution or shared cache mutation. RCU hidden hits retry through ECHILD without reference/lookup completion side effects. Native permissions and path/mount ownership remain, including post-mount inode checks. Filtered tasks take the existing separate lookup/create fallback before opening cold inodes; all nine atomic-open tables provide lookup/create functions by source inventory.

Directory filtering borrows the held file's directory, checks actual named cached children and uses ilookup/iput only within the same non-FUSE superblock. No VFS lookup or physical /data inode-number substitution occurs inside the actor. Hidden entries consume no record/buffer space, preserve filesystem cookie advancement and do not trigger inode-width/copy errors. Native visible-record, FD-position, permission/error code is retained. Removing the new guards exactly reproduces the two native files, and the dcache allocator/hash/parallel/lookup-completion implementation and native structures remain unchanged.

The aggregate SUS_PATH gate and capability exclusion remain: cache-independent FUSE enumeration is incomplete, with a marked live inode's name exposed when its child dentry is absent. The fixture explicitly demonstrates this coverage gap. Overlay/backing identities and native/concurrent/filesystem fallback semantics still require review. See [SUSFS_2_3_PATH_CONSUMERS.md](SUSFS_2_3_PATH_CONSUMERS.md) for the exact contract and next order. No upstream revision, compatibility check, version, workflow, protected branch or device operation is changed.

Four new source/host configurations pass under Clang 21.1.8, ASan/UBSan with fatal undefined-behavior reports. Whole readdir and actual namei/cache/predicate functions cover ownership, RCU retry, native permissions/errors, cold/cached opens and effects, mount transfer, task/owner gates, five record variants, empty/partial buffers, cookies, faults, signals and FUSE limits. Two source mutation controls fail as expected. Initial edit/extractor and fixture failures concerned early/final return selection, an extra guarded-helper newline and modeled baseline atomic-open error behavior; corrected without relaxing production checks. The 327-line kernel-stage diff has zero checkpatch errors/warnings; this is not a global style-clean result.

Final source review moved the completed-walk filter after successful native weak revalidation, rather than masking its errors. Added actual-function EIO/ESTALE controls, retaining successful hidden-root/dot checks.

Final checkpoint audit verifies 35 current passing host configurations: 20 affected configurations were exercised here, with all other recorded input hashes unchanged. All 125 immutable inputs, original checkout/status, protected ref, three clean pins, 14 archived prior inputs, unchanged authoritative provider and 21 native cache/structure/FUSE/SELinux/fsnotify files are preserved. The added guards restore both native edited files exactly when removed. All 114 local document links and source whitespace pass. Final source-stage checkpatch reports 0 errors, 0 warnings, 345 lines checked; existing provider/header findings remain. Evidence is .backport-context/audit/path-consumers/final-integrity.json. Production PATH rejection remains, and ARM64 build/device validation has not occurred.

## 2026-10-15 — live FUSE enumeration index

The user authorized the next step. Preserved 17 preceding inputs/results, recorded the new-file absences and saved the full preceding diff. Added an embedded, guarded FUSE live-inode index and real lifecycle hooks, selected with the path core. Generic directory emitters now find a still-live marked FUSE inode by its own superblock/full reported ino even when its named dentry has left cache. No physical /data translation, node-ID guess, allocation or retained inode/mount reference is used.

Native attribute/owner publication updates the index under fi->lock followed by an IRQ-safe index lock. Eviction unlinks before mapping teardown; direct free unlinks again. Readers keep no pointer after unlocking. The private entry is appended, preserving existing private field offsets while growing the enabled object by the aligned entry; public/native inode/vfsmount and 2.3.0 payload layouts remain. The original BPF/DAX/forget/reference cleanup and VFS RCU lifecycle are unchanged outside the new guarded calls. Known same-number unmarked/current-owner peers prevent number-based filtering.

Four new BPF/DAX configurations pass under Clang 21.1.8 with ASan/UBSan configured to halt on undefined behavior. Actual native lifecycle, owner/backing conversion, hash/list primitives and four directory parsers run under explicit host IO/allocation/locking/VFS models, including 2000 concurrent allocation/update/eviction/free cycles per configuration. Both old-cache-gap and retained-eviction-entry mutations fail; four invalid core configurations fail compilation and both built-in/module lifecycle-caller syntax probes pass. The feature-off consumer configurations still restore native source exactly. Initial harness repairs added extraction for ino_t/enum-return functions, corrected a guard-adjacent blank line, restored the refactored caller's local source read and supplied the model include path/unused probe-only fields. These were harness/source-identity issues, not ARM64 build failures.

See [SUSFS_2_3_FUSE_PATH_INDEX.md](SUSFS_2_3_FUSE_PATH_INDEX.md) for the identity/lifetime contract, actual-source evidence and limits. The reproduced live-inode/dentry-cache gap is closed at source/host level. Reported-metadata races/unknown duplicates, conservative ambiguity, global-lock contention, stacked/overlay paths and the separate open/create fallback still need production-readiness review. The aggregate SUS_PATH rejection/capability exclusion remains, and no integrated kernel/device result is claimed.

Final FUSE checkpoint audit verifies 39 current passing host configurations: the four index, four consumer, two registration, four core and six automatic SELinux configurations were exercised here; other recorded inputs still match their passing reports. All 125 immutable inputs, original checkout/status, protected ref, three exact clean source pins, 17 archived prior inputs, unchanged 16-file provider and 19 native cache/structure/parser/VFS/SELinux/fsnotify files are preserved. Guard removal restores the native FUSE source/header as well as namei/readdir exactly; the payload header remains the earlier declaration-only delta. All 127 local document links and source whitespace pass. The 188-line incremental kernel-stage diff has zero checkpatch errors and one new-file MAINTAINERS warning; pre-existing provider/header style findings remain recorded. Evidence is .backport-context/audit/fuse-path-index/final-integrity.json. Production PATH rejection remains, with no staged changes/commits, image, push, CI dispatch, flash or device operation.

## 2026-10-15 — open/create handoff and overlay identity

The user resumed SUSFS backporting after the separate non-SUSFS LineageOS/BakaSU build and storage consolidation. Read the handoff/history, verified all 125 immutable inputs and the protected reference, and preserved 24 preceding inputs/results plus the full prior diff under .backport-context/audit/path-open-create/prior-checkpoint. The SUSFS checkout now owns its independent Git directory; source pins and the authoritative 16-file provider remain unchanged.

Added a guarded final lookup_open check at out_no_open: a successful create callback can instantiate a marked inode after the earlier check, and native FMODE_CREATED handling can bypass the normal walk checks. Reject before path transfer through native dput cleanup. Native callback errors, successful-create notification, permissions and FMODE_CREATED conventions remain. This does not undo creation or remote side effects and does not close all concurrent policy changes. The consumer fixture now executes actual native open_to_namei_flags/atomic_open with modeled filesystem callbacks and caller cleanup.

New overlay host tests execute native dentry/real/backing and overlay init/update/copy/accessor helpers with actual extracted constants/enums. They preserve view-local markers across copied-up, metadata-only and nested views without marking another alias of the physical backing inode. Native overlay sources remain unchanged. Directory inode numbers do not identify the pointer-keyed overlay cache: readdir now skips generic ilookup on overlay after its named-child check. Cold overlay entries remain visible, an explicit unresolved coverage gap. The FUSE index itself is unchanged.

Four consumer, two overlay, four FUSE and four access/stat configurations pass under Clang 21.1.8 ASan/UBSan with fatal undefined behavior (14 configurations exercised here; 41 recorded overall after adding the two overlay modes). Four consumer mutations, a wrong-backing-identity overlay mutation and existing FUSE negatives fail as intended. Fixture development repairs addressed helper extraction, an invented fsnotify assertion, the native initializer's mapping local, missing unlikely, the shared WARN_ON macro and representative constants. These were host-fixture/source-extraction issues, not ARM64 failures. All four runner commands exit zero; see [the checkpoint](SUSFS_2_3_OPEN_CREATE_OVERLAY.md) for scope and limits.

The incremental kernel patch passes checkpatch with zero errors and zero warnings, 24 lines checked; older provider/header findings remain. Final preservation, current input-hash, native-source restoration, whitespace and local-link evidence is .backport-context/audit/path-open-create/final-integrity.json. No source pin, SUSFS version, BakaSU compatibility gate or production capability/rejection logic changed. Cache-independent overlay identity, other filesystem fallback assumptions, remote-create effects and native name/mount/owner/marker concurrency remain before PATH enablement. Remaining feature/mount-ID work and SUSFS ARM64/device validation are still pending. No SUSFS image, push, release, CI dispatch, flash or device operation occurred.

Final audit passes 125 immutable inputs, 24 preserved checkpoint inputs, three clean exact source pins, 41 current passing host configurations and 137 local document links. Source whitespace passes excluding the unchanged handoff and three raw diff artifacts; their pre-existing formatting is logged without rewriting immutable inputs. The separate non-SUSFS checkout's 20 original kernel-only artifact entries verify. The later boot-build output directory named in the previous handoff is absent locally, so its repacked image is not reverified here; no separate-project files were modified.

## 2026-10-15 — merged-overlay enumeration and shared-cache isolation

The user authorized the next step. Revalidated the preceding checkpoint and preserved 29 inputs/results plus the full prior diff under .backport-context/audit/overlay-merged/prior-checkpoint. Overlay source review identified merged, direct-backing and translated paths, plus a shared-cache defect: ordinary SUSFS lookup filtering can inject ENOENT into ovl_cache_update_ino while an app builds an impure/translated cache.

Added a guarded overlay-only metadata lookup helper retaining native component/hash/search-permission, revalidation, allocation/alias and reference behavior. Its private bit suppresses only the added inode visibility filter and is cleared before native lookup/revalidation callbacks. Used it for native inode-number translation and a per-caller merged-entry probe. Hidden merged entries advance native cursors without removing or relabeling shared records; ordinary path lookup still rejects the view. Native error paths remain. Direct backing iteration remains unchanged because lookup from its actor can recurse under the backing directory lock. GPL exports cover the helper and unchanged predicate body for overlay module consumers.

All six runner commands in [the checkpoint](SUSFS_2_3_OVERLAY_MERGED.md) exit zero: 20 positive host configurations exercised, 45 recorded overall, plus unchanged historical reports. Actual native merged/cache/helper/list and native/compat emitter bodies run under explicit filesystem/IO/cache/lock models using Clang 21.1.8 ASan/UBSan with fatal undefined behavior. Four new mutations fail for shared-cache policy contamination, missing merged filtering, private-flag leakage and skipped permission validation. Full actual headers pass built-in/MODULE caller syntax probes; a wrong helper prototype fails. No target modpost/link or kernel concurrency proof is claimed.

Initial repairs corrected guard-adjacent source-restoration whitespace, extraction of u64 and blank-line signatures, a native list macro dependency, generated model include resolution and caller probe declarations. Module review found and fixed the missing predicate export, then affected path-core/identity/consumer/FUSE tests were rerun. Final immutable/pin/ref/preservation/current-report hashes, guard restoration, incremental style, whitespace and local links are recorded in .backport-context/audit/overlay-merged/final-integrity.json. Imported raw patch/handoff formatting is retained unchanged.

Direct/translated backing-directory misses, failed per-entry lookups/read-without-search permission, nested policy layers, native races/lockdep/RCU and per-entry IO cost remain before PATH production enablement. Version declarations, BakaSU checks/provider, source pins, aggregate rejection/capabilities and workflows remain unchanged. Other feature bodies/ABI, mount IDs/namespaces and SUSFS ARM64/device gates still follow. No SUSFS image, push, release, CI dispatch, flash or device operation occurred.

Final merged-overlay audit passes all 125 immutable inputs, 29 preserved prior files, three clean exact source pins, protected ref, 45 current positive host configurations and 147 local links. The 172-line incremental kernel diff reports zero checkpatch errors/warnings. Source whitespace passes with only byte-preserved raw patch/handoff formatting excluded, and the separate non-SUSFS integration diff remains unchanged. The audit confines this checkpoint to its 12 expected source/test/document files and verifies exact feature-off native restoration.

## 2026-10-15 — preparation for the first limited ARM64 build

The user authorized completing all prerequisites for a limited core-only build, leaving target compilation for the next step. Revalidated the preceding checkpoint, saved eight inputs plus the full prior diff under .backport-context/audit/core-build-prepare/prior-checkpoint, and retained all 125 immutable inputs. Selected existing audited sources rather than advancing the moving upstream branch: BakaSU e1682a28 with the authoritative 16-file provider and SUSFS reference 82f08351.

Added native drivers/Kconfig/Makefile integration and a relative drivers/kernelsu symlink to a separate ignored pinned dependency checkout, preserving pristine reference repositories. New build-pins.json records 29 source/configuration hashes, 16 provider hashes, exact source/toolchain pins and action commits. The driver rejects source/dependency/compiler/config drift and unexpected kernel changes; dependency setup never resets or stashes edits. Native BakaSU revision count/version and dirty suffix remain truthful. The remote protected ref and independent availability of the exact BakaSU pin were checked read-only.

Added a core/query/logging-only fragment, root build-susfs-test.sh and a shared preparation/build driver. Real defconfig/merge/olddefconfig and native provider Kbuild checks pass with all eight unfinished features disabled, existing CFI/LTO and SELinux preserved, and susfs.o as the only SUSFS object selected. Existing native inline-checker warnings remain visible; no check or version was suppressed. The separate manual-only pinned-action workflow invokes the same build driver; it was not dispatched. The immutable legacy workflow remains unchanged.

Preparation refusal tests pass for all incomplete flags, unknown features, module KSU, missing policy/CFI, source/provider/HEAD/compiler drift and malformed Image/archive inputs. Three actual Kbuild negative configurations fail as expected. A file:// fixture verifies the CI download/extract path; the real cached official Clang archive and critical extracted members verify independently. Reviewing the earlier successful raw Image showed its memory-size field exceeds file length because of BSS, and the new collector accounts for that native format. No target build result is inferred from fixture tests.

The [build document](SUSFS_2_3_TEST_BUILD.md) gives the WSL command and exact enabled/disabled scope. Logs, actual configuration, source diff, pins, host/compiler versions and checksums are retained per run. Build failure retains incomplete status; a later successful build must validate target architecture, linked SUSFS/BakaSU evidence, devon DTBs and modules before being labeled compiled-and-linked. All 45 prior positive source/host reports still match current inputs. Final preservation/preparation/style/link evidence is .backport-context/audit/core-build-prepare/final-integrity.json. No ARM64 kernel Image/dtbs/modules target, boot repack, push, release, CI dispatch, flash or device operation occurred; the separate non-SUSFS checkout is unchanged.

## 2026-10-15 — first limited ARM64 build completed

The user authorized running the prepared build and fixing errors autonomously. Preserved 32 previous source/test/document/evidence inputs and the full pre-turn diff under .backport-context/audit/first-core-build/prior-checkpoint. The kernel base, protected working reference, three pristine source checkouts and separate non-SUSFS integration remain unchanged.

Three actual full-build attempts failed in sequence: the custom provider guard rejected native CONFIG_KSU=Y from tristate.conf during modules.builtin generation; fs/read_write.o exposed current_uid without linux/cred.h; fs/stat.o exposed filename_lookup without its existing fs/internal.h declaration. Accepted exactly y/Y for built-in KSU while preserving m/M, missing-SUSFS and wrong-kernel rejection. Added the two native header includes, preserving runtime function bodies and version/policy behavior. The 16-file provider SHA256 is now d75748e5d0a054d7ac49e56b7218f1d44ec4c6ed7ed091088469c40f7e78493c; only its Kbuild selection guard changed. Refreshed the deliberate source/provider hashes and relative dependency symlink using a new checkout, retaining the old patched checkout and pristine audit sources.

Native targeted ARM64 fs/read_write.o, fs/stat.o, fs/susfs.o and the complete drivers/kernelsu/ subtree compile with exit zero. A fourth fresh full run, 20261015T090201Z-18cxdmw2, then completes Image/dtbs/modules with exit zero using the pinned Android Clang r547379 and four jobs. Existing CFI/LTO, warnings-as-errors, SELinux, upstream checks and hook mode remain enforced. The full compiler/linker log has no warning: or error: diagnostics. The driver now records failed-stage/error/checksum evidence on future compilation or artifact-collection failures; older failed result.json files remain historical and attempts.json records their actual outcomes.

The output is a 37,519,384-byte raw ARM64 Image, 66 modules and four device-tree files. Image SHA256: 8170e2e6073a2e14ffe62fb342d32a7de2f117d2fd78567a326e96b18fff425d. Kernel release: 4.19.325-cip136-st20-perf-g18bd9752ada7-dirty. [The build record](SUSFS_2_3_FIRST_ARM64_BUILD.md) links the exact result, logs, configuration, checksums and artifacts. The independent audit validates all artifact checksums, native Image/vmlinux identities, linked provider/core symbols, module architecture/vermagic/archive identity and FDT headers/sizes/archive identity against the fresh target outputs.

All 45 current positive host configurations have matching current-input evidence after the header/provider fixture and exact-restoration updates; the historical adapter remains reproducible. Native uppercase built-in acceptance and four Kbuild rejection cases pass along with the existing preparation refusal tests. Final immutable/pin/preservation, incremental style, whitespace and link checks are recorded in .backport-context/audit/first-core-build/final-integrity.json. This milestone enables only SUSFS core/query/logging with BakaSU; all eight unfinished optional features, production rejection and truthful capability limits remain. Full feature/ABI, direct-overlay, mount-ID/namespace and native concurrent/runtime validation are still pending. No commit, push, release, CI dispatch, boot-image repack, flash or device operation occurred.

## 2026-10-15 — eight optional features and combined ARM64 build

The user authorized implementing all eight optional features. Saved the entire
preceding diff and 88 source/test/document inputs under
`.backport-context/audit/all-features/prior-checkpoint`. Preserved the 125
immutable handoff/source inputs, protected reference, pristine source pins,
16-file BakaSU provider, original core build and separate non-SUSFS project.

Implemented uname, cmdline and proc-symbol filtering, SUS_MAP consumers,
native/compat KSTAT rules and snapshots, and owned-filename OPEN_REDIRECT
with all five UID schemes. Completed direct/translated overlay PATH iteration
using pending records and deferred metadata lookup. Added coherent mount
classification, copied-view IDs and native allocation/clone/free consumers,
plus mount/proc/statfs/fdinfo/name-to-handle and statx mount-ID integration.
The [feature record](SUSFS_2_3_ALL_FEATURES.md) specifies contracts, deliberate
reference fixes and limits; native propagation and Motorola data callbacks
remain in their original paths.

Review found an ID-reuse bug in the first physical/presentation-ID design.
Added refcounted IDA-slot owners so namespace copies keep original displayed
IDs reserved after the original mount dies, without pinning that mount.
Allocation occurs before release, and unwind puts each owned/view reference
once. Native ARM64 objects and SMP/non-SMP host ownership/failure cases pass;
five broken ownership/filter mutations are rejected. The earlier pre-fix
object diagnostic is retained and does not establish runtime safety.

Current source/host validation passes 65 positive configurations plus seven
historical f443 adapter configurations. Combined KSTAT/MOUNT tests preserve
metadata snapshots and native LSM/filesystem errors, balance visible-root
references and check mount-ID result masks. All 12 command payloads match
pinned JNI sizeof and every field offset on ARM64/ARM32; explicit kstat EABI
conversion and the 256-byte/byte-144 statx extension also pass target probes.
Prior source-restoration tests now remove the new balanced feature guards;
unknown mixed elif conditions remain visible rather than silently evaluated.
Earlier generated-header diagnostics and fixture/expectation failures remain
recorded separately from kernel build results.

The source-pinned build driver retains the core default and adds all/eight
individual profiles, exact object/config checks, 55 reviewed kernel inputs,
source archives including new files, and a post-build source/provider check.
Each individual profile passes real configuration and native BakaSU Kbuild
preparation. Source/provider/compiler drift, module KSU, absent policy/CFI,
unknown features and malformed Image/archive checks still reject as expected.
No upstream check or version has been bypassed or falsified.

All-eight run `20261015T124521Z-11i5pjlo` completed Image/dtbs/modules with
CFI/LTO and passes independent artifact checks: 37,521,432-byte ARM64 Image,
SHA256 `d346326d668b8962b8e61fd37b46feb69b28bb751870dfb02db6f0fa81f68c53`,
66 AArch64 modules and four FDTs. The current-source core regression run
`20261015T124900Z-gemjv5a5` also compiled and linked successfully, producing a
37,519,384-byte Image, 66 AArch64 modules and four FDTs; Image SHA256
`36b297a8bd037ef0e9ca2dbec30706ce5673d4d1a47fa8e4c1d86754774da5b1`.
Individual preparation and targeted objects are not described as eight full
Image links.

All eight have conditional implementations and truthful capability output;
the final local unfinished-feature aggregate rejection is removed. Unsupported
AVC spoofing remains `EOPNOTSUPP`. Incremental checkpatch retains two errors
and 12 warnings, documented in the feature record; it is not reported as a
clean style check. Whitespace/preservation checks pass. Source remains
uncommitted, and the manual-only core workflow is unchanged and undispatched.
No boot image, push, release, flash or device test occurred. Native concurrency,
boot, module/ROM compatibility and runtime behavior remain unvalidated.

Final `.backport-context/audit/all-features/final-integrity.json` passes all
125 immutable inputs, 88 preserved preceding inputs, exact source/provider
pins, current host/ABI/refusal report hashes, both complete build artifacts,
eight individual preparations, native layout/callback preservation, 172 local
links and unchanged protected/separate non-SUSFS state. Neither style cleanup
nor device runtime is marked passed. All modifications remain reviewable local
work with no staged contents or commits.


## 2026-10-15 — core-only temporary boot reached Android

The user explicitly authorized `fastboot boot` only, after the earlier all-eight
image returned to bootloader. That failed image SHA256 is
`0255a49465a3cc7a098ebd6ecf1822d9947926b33e85cf4cdb6ac792bd88c7b2`;
its saved pstore/last_kmsg diagnostics did not establish a crash cause. An
unchanged installed-boot-image control subsequently reached Android on the
old `g80f0ee4d1ec6-dirty` kernel. Neither test flashed a partition.

Extended only the repacking utility to select an explicitly validated core
or all profile, retaining source/provider/build/configuration refusal checks.
No kernel feature implementation or compatibility/version check changed.
Packed the already completed current-source core build
`20261015T124900Z-gemjv5a5` into the hash-verified original Axion image,
preserving ramdisk and boot settings. Pinned AOSP unpacking and unsigned AVB
hash/layout verification passed; the separate modules archive was not injected.

Run `artifacts/susfs-features-test/boot-tests/20261015T164903Z-cio7gbbs`
contains the image, manifests, fastboot log, running configuration, kernel
logs and independent validation. Boot image SHA256:
`4267e32cc2429afa2583a499a783aee15347e5caf33de49f49fbd3cc4d8ffffe`.
`fastboot -s ZD222C45X9 boot <core image>` returned exit zero, and actual
Android startup was independently verified: `sys.boot_completed=1`, user
unlock/CE storage available, running release
`4.19.325-cip136-st20-perf-g18bd9752ada7-dirty`. Running `/proc/config.gz`
confirms built-in BakaSU/SUSFS core with all eight optional features off,
CFI/LTO enabled and CFI permissive disabled. BakaSU root works; SELinux is
Enforcing. BakaSU pin remains `e1682a2831142e68fa30bab97d5690cf90b75119`
with the documented provider patch; SUSFS reference remains
`82f08351fadb8b19dd6591bd14634d02ad832145`.

The installed active boot_b SHA256 remained
`abbf327aadd8ae96e22913252ac75fbcb53ec9345d51ef681cd34b0c3f91b093`;
slot b was unchanged. Existing vendor/device modules loaded normally; this
was not a module-free kernel or proof that root-manager modules were disabled.
Early logs contain no detected panic/Oops/BUG/CFI failure or module
version/symbol failure signatures. Display duplicate-sysfs and memcg LRU
accounting warnings also occur in the working-kernel control and remain
recorded as unresolved baseline findings. The initial ADB polling classifier
missed variable whitespace; its result was corrected against preserved raw
observations and verified runtime data, without inventing a boot failure.

This is a single successful temporary core-only boot and limited early-runtime
check. It narrows the failed all-eight investigation toward optional-feature
code or interactions; it does not identify the offending feature, establish
full SUSFS behavior, validate hardware/stability, or authorize flashing.
Next: controlled individual-feature or small-group builds/tests against this
successful baseline. No further image was booted, pushed or released.


## 2026-10-15 — uname and command-line feature profile booted

The user requested the next image with a small set of likely-working features
and moved the phone into bootloader. Selected SPOOF_UNAME and
SPOOF_CMDLINE_OR_BOOTCONFIG: their default-inactive handlers have fewer VFS,
mount-ID and MM interactions than the remaining optional features. Added an
explicit `identity` build profile and let the packer accept reviewed named
profiles with distinct filenames. All old profiles, 55 kernel input hashes,
BakaSU provider hashes/checks, versions, enforcing CFI/LTO, original ramdisk
and protected branch remain unchanged. No feature implementation was edited.
Saved previous utility/pins/document inputs under
`.backport-context/audit/identity-boot-20261015T1655Z`.

A fresh `build_test.py --build --profile identity --jobs 4` run
`20261015T165721Z-ri3pre41` completed Image/dtbs/modules with exit zero.
Image SHA256 `eb55e6ff618fd38c8051a3e368bd2d35bb3f497e0fa6a890e2edf063fd532148`;
37,519,384 bytes, 66 AArch64 modules, four valid FDTs. Independent checks pass
all 24 artifact checksums, matching module vermagic, four selected linked
functions and no compiler/linker warning:/error: diagnostics. Actual config
differs from the successful core build in exactly those two feature flags.
Focused real-handler/native-consumer ASan/UBSan host models pass for the exact
two-feature combination. Configuration/source/provider/compiler refusal tests
and four native Kbuild negative cases pass; prior evidence is preserved.

Packed the original hash-verified Axion boot image and independently validated
it with pinned AOSP unpack/AVB tools. Test image SHA256
`74acd148fdf9c67d53ddac74bc1cac157874eb5a2baec673096303ccc08fc243`;
file `boot-bakasu-susfs-identity-test.img`, 96 MiB. The full record is
`artifacts/susfs-features-test/boot-tests/20261015T171237Z-k28qqoai` and a
Windows copy under `D:/Phone expriment/Moto G32/Axion/SUSFS-Identity-Test/20261015T171237Z`.
The separate modules archive was not injected; existing vendor/device modules
loaded normally. No claim that root-manager modules were disabled.

`fastboot -s ZD222C45X9 boot <identity image>` returned exit zero. Actual
Android boot completion and user unlock were verified, with expected kernel
`4.19.325-cip136-st20-perf-g18bd9752ada7-dirty`, running config byte-identical
to the compiled identity config, BakaSU root available and SELinux Enforcing.
The active boot_b SHA256 remains
`abbf327aadd8ae96e22913252ac75fbcb53ec9345d51ef681cd34b0c3f91b093`;
slot b unchanged. No partition was flashed. Final log assessment and raw
kernel logs preserve all warning findings; no crash cause is inferred merely
from matching baseline warnings.

This validates a single temporary boot and limited early runtime with these
two optional features compiled/enabled. No live spoofing values were set by
this task, so functional spoofing, concurrency, long-term stability and the
six other optional features remain pending. The all-eight failure remains
unresolved; this result narrows further isolation without identifying a
culprit. No push, release, CI dispatch or additional device boot occurred.


## 2026-10-15 — symbol hiding and open redirect profile booted

The user authorized another feature group and ADB reboot into bootloader,
then explicitly withdrew the initial offer to flash: **fastboot boot only**.
Recorded this restriction before completing the build. Added only a named
`redirect-symbols` profile containing the prior SPOOF_UNAME/cmdline features
plus HIDE_KSU_SUSFS_SYMBOLS and OPEN_REDIRECT. No kernel implementation,
provider, version or compatibility check changed. The four remaining optional
features stay off. Prior inputs/diff, exact two-flag config comparison, host
results and build validation are saved in
`.backport-context/audit/redirect-symbols-20261015T1718Z`.

Fresh build `20261015T171901Z-0wpz7y67` completed Image/dtbs/modules with
exit zero and enforcing CFI/LTO. The 37,519,384-byte Image SHA256 is
`6385e9c07b2643bd1e6ab14ca94cf2971082237a71d36e78fbf40c7bfa269b2c`.
Independent checks pass 24 artifact checksums, nine selected linked functions,
66 AArch64 modules with matching vermagic and four valid FDTs, with no
compiler/linker warning:/error: diagnostics. Exact four-feature identity and
redirect ASan/UBSan host models pass; their filesystem/IO/lock/concurrency
models do not establish native runtime safety. Configuration/source/provider/
compiler refusals and four native Kbuild negative cases pass.

Packed the hash-verified original Axion ramdisk/settings/AVB flags unchanged.
Pinned AOSP unpack and unsigned hash/layout verification passed. Run
`artifacts/susfs-features-test/boot-tests/20261015T173437Z-2_ldacea` contains
`boot-bakasu-susfs-redirect-symbols-test.img` (96 MiB), SHA256
`cea761ef30f4bcaf9e8ceface78188afac5819b3ae1375323730d21043d1cfa6`.
Windows copy: `D:/Phone expriment/Moto G32/Axion/SUSFS-Redirect-Symbols-Test/20261015T173437Z`.
No separate modules archive was injected; existing vendor/device modules
loaded. No claim that root-manager modules were disabled.

ADB rebooted the authorized device to bootloader; product devon and slot b
were verified. `fastboot -s ZD222C45X9 boot <image>` returned zero, then
Android reached boot_completed=1. Running kernel release remains
`4.19.325-cip136-st20-perf-g18bd9752ada7-dirty`; the running config matches
the new compiled config byte-for-byte. BakaSU root works; SELinux Enforcing.
User unlock/CE availability was verified.
The installed boot_b SHA256 remained
`abbf327aadd8ae96e22913252ac75fbcb53ec9345d51ef681cd34b0c3f91b093`.
No flash, slot change, push, release or CI dispatch occurred.

Read-only live symbol filtering passes: targeted core entries decrease from
425 in the identity baseline to zero, while 184445 other core symbol entries
remain readable. Raw symbol addresses were not saved. This is functional
proc-filter evidence in addition to startup/config/root checks. See the run's
symbol-visibility-test.json, post-boot-status.json, raw logs and final
log-assessment.json for exact observations and warnings.

No live redirect or uname/cmdline spoofing rules were installed by this task.
Those functional paths, native races/concurrency, hardware and longer-term
stability remain pending, as do SUS_PATH, SUS_MOUNT, SUS_KSTAT and SUS_MAP
boot/runtime tests. Four optional features now coexist through this single
temporary boot; the earlier all-eight failure is still unresolved. BakaSU pin
`e1682a2831142e68fa30bab97d5690cf90b75119`, SUSFS reference
`82f08351fadb8b19dd6591bd14634d02ad832145` and protected branch remain
unchanged. All changes stay local and uncommitted.


## 2026-10-15 — stat and map profile booted with six optional features

The user requested the next two features and another fastboot boot. Selected
SUS_KSTAT and SUS_MAP on top of the four successfully booted options, leaving
SUS_PATH and SUS_MOUNT off for controlled isolation of the earlier all-eight
failure. Added only the `metadata-map` profile; no kernel implementation,
provider, compatibility/version check, protected branch or workflow changed.
Saved previous inputs/diff and exact two-flag config comparison under
`.backport-context/audit/metadata-map-20261015T1740Z`.

Fresh `build_test.py --build --profile metadata-map --jobs 4` run
`20261015T174124Z-0quckmn1` compiled/linked Image/dtbs/modules successfully.
The 37,519,384-byte Image SHA256 is
`ee98f64a738b896a96743c70c0f608ed75baafca9fedfda439ff4d17f0a80a46`.
Independent checks pass 24 artifact checksums, 16 selected linked functions,
66 AArch64 modules with matching vermagic and four valid FDTs, with no
compiler/linker warning:/error: diagnostics. CFI/LTO remain enforcing.
Focused real identity/redirect/kstat/map bodies pass ASan/UBSan host models
with the exact six feature macros; retained ARM64/ARM32 kstat JNI/compat
layout probes pass. Configuration/source/provider/compiler refusal tests and
four native Kbuild negative cases pass. These do not prove native concurrency
or live metadata/map rule behavior.

Packed the original hash-verified Axion ramdisk/settings/AVB flags unchanged;
pinned AOSP unpack and unsigned hash/layout verification passed. Run
`artifacts/susfs-features-test/boot-tests/20261015T175505Z-66y2u3sy` contains
`boot-bakasu-susfs-metadata-map-test.img` (96 MiB), SHA256
`ab14dc81e8acba7fa62df1a9c1e8b2c4a2d629d9bf5bd7d4dc34d13b1c3755cd`.
Windows copy: `D:/Phone expriment/Moto G32/Axion/SUSFS-Metadata-Map-Test/20261015T175505Z`.
No separate modules archive was injected; existing vendor/device modules
loaded. No root-manager-module-free claim.

ADB rebooted the authorized phone into bootloader; product devon and slot b
were verified. `fastboot -s ZD222C45X9 boot <image>` returned zero, then
Android reached boot_completed=1. The running
`4.19.325-cip136-st20-perf-g18bd9752ada7-dirty` kernel configuration matches
the compiled six-feature config byte-for-byte. BakaSU root works; SELinux
reports Enforcing. User unlock/CE availability was verified.
Read-only symbol filtering still passes: zero targeted core entries, with
184445 other core entries visible. Raw addresses were not saved. Installed
boot_b remains SHA256
`abbf327aadd8ae96e22913252ac75fbcb53ec9345d51ef681cd34b0c3f91b093`;
slot b unchanged. No flash, push, release or CI dispatch occurred.

The run preserves initial/later kernel logs, warning comparisons, runtime
configuration, manifests and actual post-boot status. No live metadata/map,
redirect or uname/cmdline rules were applied by this task. Six optional flags
now coexist through this single temporary boot and early runtime; rule
behavior, hardware, native races and stability remain unvalidated. PATH and
MOUNT are the two optional flags still disabled. Test them separately next;
this result does not yet identify the culprit in the failed all-eight image.
BakaSU pin `e1682a2831142e68fa30bab97d5690cf90b75119`, SUSFS reference
`82f08351fadb8b19dd6591bd14634d02ad832145`, provider and protected branch
remain unchanged. Prior successful core, identity and four-feature images are
preserved. All modifications remain local and uncommitted.


## 2026-10-15 — SUS_PATH booted with SUS_MOUNT disabled

The user explicitly chose SUS_PATH next and SUS_MOUNT last, with temporary
fastboot boot only. Added only the `path-no-mount` profile: previous six flags
plus SUS_PATH, SUS_MOUNT=n. The actual configuration differs from the successful
six-feature build in exactly that one flag. No kernel/provider implementation,
version/check, protected branch, toolchain or workflow changed. Previous
inputs/diff, plan, host runners/results and artifact validation are saved in
`.backport-context/audit/path-boot`.

Fresh `20261015T180241Z-5kjdtl8a` Image/dtbs/modules build passed with CFI/LTO
enforcing. Image: 37,521,432 bytes, SHA256
`c6ab83c441bf5f9ff2993c72a346b955247d27c578af3e119cb85616864b7284`.
Independent validation passes 24 artifact checksums, 25 selected linked
functions including PATH/FUSE, 66 AArch64 modules with matching vermagic and
four FDTs; no compiler/linker warning:/error: diagnostics. Preparation refusals
and four native Kbuild negative cases pass. Existing PATH core/consumer,
overlay and FUSE host suites were rerun into isolated output directories;
only runner output locations were changed, and used drivers/hashes are saved.
Their modeled lifetime/concurrency/IO limitations remain explicit.

Packed the hash-verified original Axion ramdisk/settings/AVB flags unchanged;
pinned AOSP unpack and unsigned hash/layout verification passed. Run
`artifacts/susfs-features-test/boot-tests/20261015T181547Z-8wtkupgk` contains
`boot-bakasu-susfs-path-no-mount-test.img` (96 MiB), SHA256
`3a23c9568ecc94a2a68334575fbf20f4d0a47b33545e37e692b64a9043b315ef`.
Windows copy: `D:/Phone expriment/Moto G32/Axion/SUSFS-Path-Test/20261015T181547Z`.
No separate modules archive was injected; existing vendor/device modules
loaded. No root-manager-module-free claim.

ADB rebooted the phone into bootloader; product devon and slot b verified.
`fastboot -s ZD222C45X9 boot <image>` returned zero; actual Android startup
completed. Running kernel release
`4.19.325-cip136-st20-perf-g18bd9752ada7-dirty` and byte-exact running
configuration verify SUS_PATH=y and SUS_MOUNT=n alongside the six previous
features. BakaSU root works, SELinux reports Enforcing. User unlock remained pending at the last observation.
The installed boot_b hash remains
`abbf327aadd8ae96e22913252ac75fbcb53ec9345d51ef681cd34b0c3f91b093`;
slot b unchanged. Symbol filtering passes with zero targeted core names and
184463 other core entries visible; raw addresses were not saved. No flash,
push, release, CI dispatch or additional device boot occurred.

Comparison with the prior failed all-eight build verifies identical kernel,
provider and toolchain pins; the sole actual config difference is SUS_MOUNT.
This strengthens suspicion of SUS_MOUNT or its interactions, without a failing
crash trace or standalone-MOUNT diagnosis. Current logs/status/warning
comparisons are preserved; live path hiding, other rule behavior, hardware,
native races and long-term stability remain unvalidated. No live rules were
set by this task. Seven optional features now coexist through one temporary
boot and early runtime; SUS_MOUNT remains disabled. Review its implementation
and test it separately before drawing a definitive cause conclusion.

BakaSU `e1682a2831142e68fa30bab97d5690cf90b75119`, SUSFS reference
`82f08351fadb8b19dd6591bd14634d02ad832145`, provider and protected branch
remain unchanged. Prior successful images and the failed all-eight test are
preserved. Source remains local and uncommitted.


## 2026-10-16 - isolated SUS_MOUNT retry and installed-module ABI finding

The user confirmed that the previous seven-feature SUS_PATH=y/SUS_MOUNT=n
image reached usable Android after unlock. A fresh read-only check verified
CE storage available, BakaSU root and enforcing SELinux, with the exact seven
feature configuration. Saved a byte-verified copy to
`D:/Phone expriment/boot-susfs-7features-working-no-mount-20261016.img`, with
SHA256 `3a23c9568ecc94a2a68334575fbf20f4d0a47b33545e37e692b64a9043b315ef`
and adjacent checksum/provenance files. The original supplied boot remains
unchanged. Prior seven-feature raw observations remain historical records;
its user-confirmed-boot-20261016.json records the later successful unlock.

The user requested enabling SUS_MOUNT and capturing the failure. Used the
existing all profile with unchanged kernel/provider/compiler inputs. The
actual configuration differs from the working seven profile only by
KSU_SUSFS_SUS_MOUNT=n -> y. Fresh build
`20261015T183754Z-ltiekays` passed compilation/linking of Image, 66 AArch64
modules and four FDTs, enforcing CFI/LTO, 24 artifact checksums and 29 selected
linked functions. Image SHA256:
`d76537b43f6bbf3f3131468af3cd969c7a8b496cec70cd4a6f761e62aae522cf`.
Preparation/refusal tests pass and are saved separately in
`.backport-context/audit/mount-boot-20261016/preparation-tests`.
No compiler/linker warning:/error: diagnostics were found. Compilation does
not establish installed-module compatibility or boot safety.

Repacked the unchanged Axion ramdisk/settings/AVB flags and independently
validated kernel/ramdisk hashes and unsigned NONE AVB layout using the pinned
AOSP tools. Boot run:
`artifacts/susfs-features-test/boot-tests/20261015T185228Z-h9x2q4j6`.
Test image SHA256:
`4be0dfe75065a1a2335ef95bea2ff59d629884361e45335a374a4f8b082483e6`.
Windows image copy is under
`D:/Phone expriment/Moto G32/Axion/SUSFS-Mount-Test/20261015T185228Z`.
No separate modules archive was injected. No version, CFI or module CRC
checks were bypassed.

Offline checks found an installed-module ABI incompatibility. SUS_MOUNT adds
mnt_id to native struct kstat in include/linux/stat.h. The mount-enabled
build changes 7393 common exported symbol CRCs relative to the working seven
build. A read-only copy of /vendor/lib/modules/mmi_info.ko has module_layout
CRC 0x7b99c8ef, identical to the working seven kernel; the old and fresh
mount-enabled builds use 0x81f987d1. All 42 checked module imports match the
seven build, while 14 mismatch the mount-enabled build. The working kernel's
saved startup log shows mmi_info loading during first-stage init. The original
boot ramdisk contains no modules; this check does not independently identify
the exact /lib/modules binary supplied by vendor_boot. ABI mismatch is proven
for the sampled installed driver, while the actual failed-boot path still
needs a failing-kernel log. Detailed CRC evidence is retained in the audit.
The intended repair direction is preserving native exported kstat ABI and
keeping mount-ID reporting in a private statx/helper path, followed by module
CRC validation before another boot. No such source repair was made here.

Before the test, verified devon/slot b, root, running configuration and unchanged
installed boot_b SHA256
`abbf327aadd8ae96e22913252ac75fbcb53ec9345d51ef681cd34b0c3f91b093`.
Started a timestamped read-only USB/log observer before fastboot boot. The
transport command returned zero, but the phone returned to bootloader without
ADB appearing. Bootloader reason: "Reboot mode set to fastboot"; slot b remains
bootable. No crash trace was exposed by that test. The observer's transport
and actual-startup results are recorded separately.

Temporarily booted the hash-verified saved seven-feature image for recovery,
again with an observer already running to capture pstore at first root. That
fastboot command also returned zero. The user confirmed Android/lock screen was running. USB initially remained
absent, including after a host ADB service restart. After the user changed
USB port and selected File transfer, ADB/root returned. Verified exact
seven-feature running config, Android boot completion, user unlock, BakaSU
root, enforcing SELinux and unchanged installed boot_b hash. No slot changed.

Recovery pstore is empty, /proc/last_kmsg is absent and DropBox reports no
SYSTEM_LAST_KMSG entries. The USB delay means the first-root pstore capture
was after Android startup; no pre-BootReceiver capture is claimed. Recovery
dmesg belongs to the working seven kernel, includes the previously observed
memcg LRU warning and has no detected panic/Oops/BUG/CFI or module-version
failure. It is not a trace of the failed mount-enabled kernel. The independent
module ABI mismatch remains the strongest concrete finding; the exact reboot
path is still unproven. See the run's log-assessment.json and Windows
mount-boot-diagnostics.zip for the final evidence.

The bounded capture helper was first tested on the running seven-feature
phone: live dmesg/logcat capture and root/config access worked; baseline
pstore was empty. Ramoops console/panic logging remains enabled with the
existing memory layout and panic/watchdog settings. A pre-ADB failure or
firmware clearing retained RAM may still leave no saved trace. All task
evidence is in the boot run and
`.backport-context/audit/mount-boot-20261016`. No partitions were flashed,
slots changed, source compatibility checks bypassed, or images pushed or
released. Protected devon-resukisu and the separate Lineage checkout remain
untouched. All changes remain local and uncommitted.


## 2026-10-16 - SUS_MOUNT native ABI repair

The user authorized fixing SUS_MOUNT. Preserved the prior source, pins,
host reports and patch under
`.backport-context/audit/mount-abi-fix-20261016T045002Z`.
Kernel base remains 18bd9752ada79090ce11326662eaaa9f33717728, BakaSU remains
pinned to e1682a2831142e68fa30bab97d5690cf90b75119 with the existing provider
patch, and SUSFS reference remains 82f08351fadb8b19dd6591bd14634d02ad832145.
No provider, compiler, version, compatibility guard, CFI setting or workflow
changed. Protected devon-resukisu and the separate Lineage checkout are untouched.

Restored native struct kstat exactly and transported the statx mount ID through
private optional out-parameters while preserving public VFS prototypes. The ID
is selected while the resolved path is referenced; SUS_KSTAT metadata and saved
ID still come from one RCU rule snapshot. Only cp_statx fills stx_mnt_id and
advertises STATX_MNT_ID. Userspace statx remains 256 bytes, mount ID at offset
144; legacy native kstat callers do not advertise a field they cannot carry.

A first complete all-feature build, 20261016T045417Z-8c9o0ubn, restored
module_layout to 0x7b99c8ef and all 42 sampled installed-driver imports, but
complete exported-CRC comparison found 126 remaining differences out of
12,546 common exports. This intermediate build was not repacked or booted.
Those differences came from extra SUSFS members in struct mount, visible in
translation units including the private mount header.

Restored fs/mount.h exactly to native too. fs/namespace.c now allocates a
private struct susfs_mount from the existing mount cache, with native struct
mount first and SUSFS ID/class/owner data following it. VFS object layout and
exported type graph stay native; existing allocation, namespace-copy references,
unwind and free paths retain ownership. The mount cache size includes the
private data; no extra per-mount allocation failure point is introduced.
Six reviewed source hashes changed in build-pins.json; all other pins and
feature selections are unchanged. The all profile still enables all eight
options. See docs/SUSFS_2_3_MOUNT_ABI_FIX.md and the incremental kernel patch.

Focused tests pass five actual statx-body host configurations, four ARM64/ARM32
layout probes and six negative controls (lost IDs/transport, LSM bypass, early
path release and the old enlarged kstat layout). Existing KSTAT and access/stat
fixture suites pass. Native mount allocator/clone/unwind models pass with SMP
off/on and MOUNT off/on, with six rejected mutations including an undersized
cache. Disabled mount init/allocation bodies and both restored headers match
native source. Three target ARM64 objects (stat, susfs_kstat, namespace) also
compile with MOUNT disabled, without compiler diagnostics; this is not a full
mount-disabled Image build. Strict incremental checkpatch reports zero errors,
warnings or checks. Focused source whitespace/syntax checks pass; whole-tree
whitespace checks flag unchanged historical patch-file context lines and are
not misreported as passing.

Added check_module_abi.py as a read-only full-export/import gate. Real artifact
controls accept the working seven build, reject the original failed all build
(7,393 changed exports and 14 sampled import mismatches), and reject the first
repair (126 changed exports despite zero sampled import mismatches). Matching
module_layout or one driver alone is insufficient. New tests/probe sources
are preserved in reviewed-code.tar.gz because Git diffs omit untracked files.

The second fresh all-feature build is 20261016T050934Z-ko0pylez. Its generated
object CRCs match all 12,546 working-seven exports, including the prior 126
mismatches. Final linked artifact checks and device validation are pending.
No source was edited during either build. Native Kbuild/config/source/provider/
compiler refusal tests pass for the final source selection.

The phone initially was offline, then the user connected/unlocked it. Read-only
inspection found installed boot_b now equals the saved working-seven image,
SHA256 3a23c9568ecc94a2a68334575fbf20f4d0a47b33545e37e692b64a9043b315ef.
The user explicitly confirmed installing it between sessions. This external
change is the new recovery baseline; it was not flashed by this task. A normal
fastboot reboot can recover to that verified installed image if necessary,
without another upload that might disturb retained RAM.

Prepared a 3,224-byte freestanding ARM64 read-only probe using the actual statx
UAPI declarations. Its SHA256 is
9c35b5b871bd77be9af20cfb3aad4600d6a7922857e1173816a0200a5a0dc534.
It compares pathname/AT_EMPTY_PATH statx with own-fdinfo IDs and inode identity
for five public paths. It changes no mount/rule state. The installed seven
kernel provides the expected negative control: all regular statx operations
succeed but STATX_MNT_ID is unavailable. The uniquely named temporary probe
is in /data/local/tmp pending final test/cleanup. This does not test actual
hiding rules, app namespaces, propagation races, hardware or long-term stability.

No partition flashing, slot change, push, release or CI dispatch occurred.
All source changes remain local and uncommitted. Final build/boot results will
be recorded below after execution.

### Final repair build and successful temporary all-feature boot

Fresh 20261016T050934Z-ko0pylez compiled/linked Image, 66 AArch64 modules
and four FDTs with no compiler/linker diagnostics. Image SHA256:
4653f9f676ca6eea80cfe97dad0120a9233ea52e8ade1d745da0d547c17b74b8.
All 24 artifact checksums and 29 selected linked functions pass. The complete
export gate passes all 12,546 reference CRC/kind/namespace entries, none
missing; all 42 sampled installed-driver imports match. CFI/LTO remain
enforcing, MODVERSIONS enabled, native headers exact. Configuration differs
from working seven only by KSU_SUSFS_SUS_MOUNT=n -> y; source has the six
reviewed ABI repairs. The first repair's 126 mismatches are retained as an
explicit negative result; it was never repacked or booted.

Packed and independently verified the original Axion ramdisk/settings/AVB
flags unchanged. Run:
artifacts/susfs-features-test/boot-tests/20261016T052359Z-ssfra2t_.
Boot image SHA256:
2cebd0b4869c60497453c88e3310b9510fb53201b0d58a56efedc9a777c79490.
Windows image:
D:/Phone expriment/Moto G32/Axion/SUSFS-Mount-Fix/20261016T050934Z/boot-bakasu-susfs-test.img.
Pinned AOSP unpack and unsigned NONE hash/layout checks pass. No modules
archive was injected, no verification flags/version checks were bypassed.

After verified devon/slot-b preflight, fastboot boot returned zero. The phone
reached Android and the user unlocked it. Independently verified boot
completion, CE storage, root, SELinux Enforcing, exact all-eight running
configuration and unchanged installed boot_b SHA256
3a23c9568ecc94a2a68334575fbf20f4d0a47b33545e37e692b64a9043b315ef.
Running boot ID: bd95debb-fca1-4f64-906d-7bc8be74711d.
No recovery boot was needed and no partition was flashed. The currently
running all-feature image is temporary; installed seven remains the reboot
baseline the user installed independently.

Read-only runtime probe passes five public paths in Shell and root (ten
cases): pathname statx, empty-path fd statx and own fdinfo mount IDs agree;
inode identity agrees. The seven baseline had ordinary statx working but
mount-ID support unavailable, providing the expected live negative control.
The unique temporary device probe was removed and absence verified.
Sixty-three modules are Live, including mmi_info and wlan. Symbol filtering
shows zero targeted core entries and 184459 other visible core entries;
no raw symbol addresses saved. Module presence is not hardware testing.

Final logs show no detected panic/Oops/BUG/CFI, module-version/unknown-symbol,
refcount or sanitizer failure. Existing memcg LRU and display duplicate-sysfs
warnings persist and remain documented. Pstore is empty. No full hiding rules
were applied: app-namespace behavior, propagation/native races, hardware and
longer-term stability are still unvalidated. This successful startup follows
both ABI repairs; the exact previous fastboot reboot path remains without a
saved failing-kernel trace.

Manifests, running config, transport/startup timelines, kernel/crash logs,
module/ABI reports, smoke results and incremental source patch are preserved
in the boot run and audit, with Windows mount-fix-diagnostics.zip. Reviewed
untracked tests/checker/probe are included in reviewed-code.tar.gz. The
protected branch, separate Lineage checkout, original supplied boot and all
previous working images remain unchanged. Source remains local/uncommitted.


## Installed-image follow-up: 2026-10-16

The user explicitly authorized flashing the previously successful fixed image.
Verified device ZD222C45X9/devon, active slot b and 96 MiB boot_b partition.
Before flashing, installed boot_b matched the saved working-seven recovery
image (3a23c9568ecc94a2a68334575fbf20f4d0a47b33545e37e692b64a9043b315ef).
Verified the exact tested image hash immediately before writing. Fastboot
flash boot_b completed successfully; slot selection was unchanged. No other
partition was written, and no disable-verity/verification command was used.

After reboot the user reported Android/lock screen, then unlocked/reconnected
USB. Independently verified Android boot completion, CE storage available,
slot b, root and SELinux Enforcing. Installed boot_b SHA256 now equals the
fixed image: 2cebd0b4869c60497453c88e3310b9510fb53201b0d58a56efedc9a777c79490.
The running config is byte-identical to the successful temporary all-feature
boot, including SUS_MOUNT=y; SHA256 9d0f57fdae59352f49972f8d60dcf774559293d3c8558d588e093cafd6ddb33b.
New boot ID: a287241f-c5d1-4ccb-9323-91bdeb3f81df.
This is now the installed reboot baseline, superseding the earlier temporary-
boot state described above. Actual hiding-rule and longer stability tests
remain pending; installation/startup do not establish feature correctness.
The original supplied boot and saved working-seven recovery image remain
unchanged. This follow-up did not change kernel source, build or push.

Install command/transport/device/config/log evidence:
.backport-context/audit/flash-mount-fixed-20261016T053345Z.


## 2026-10-16: obsolete build and test-image cleanup

After the fixed all-feature image was installed and booted, the user requested
removal of obsolete files, including old boot images and development outputs.
Removed all nine WSL compilation/preparation caches, superseded Image/module/
DTB archives, old debug ELFs, duplicate WSL boot images, Python bytecode, the
superseded generated BakaSU provider and the verified compiler download archive.
The complete extracted pinned compiler and active patched provider remain.
Eight old Windows test/control boot images were removed individually. Original
user backups and the separate non-SUSFS project's source remain untouched.

The prior root-level recovery-image copy was absent. Before deleting the old
path-test image, verified its seven-feature hash and copied it to:
D:/Phone expriment/Moto G32/Axion/SUSFS-Mount-Fix/20261016T050934Z/recovery-7features-no-mount.img.
Current installed image remains:
D:/Phone expriment/Moto G32/Axion/SUSFS-Mount-Fix/20261016T050934Z/boot-bakasu-susfs-test.img.
Both image hashes and the original supplied boot were rechecked after deletion.
Preserved current Image/modules/DTBs/config/Module.symvers/System.map, build and
boot logs, source snapshots and ABI evidence. The exact installed kernel's
vmlinux was compressed and verified by streaming decompression/SHA256, at:
artifacts/susfs-features-test/debug-symbols/20261016T050934Z-ko0pylez/vmlinux.zst.
The raw out/vmlinux path is intentionally removed; the next full build recreates
compilation caches. Historical artifact manifests are unchanged, with separate
CLEANUP.json markers identifying deliberately removed binaries.

Before updating this log, verified all 75,558 SUSFS and 75,469 Lineage source/
symlink entries unchanged, along with Git indices and every ref. Kernel code,
build scripts, patches, tests and the protected devon-resukisu ref are unchanged.
Only this cleanup documentation was added afterward. No phone operation occurred.

Net measured Linux allocation reduction: 36,907,237,376 bytes.
Net Windows image reduction after retaining the recovery copy:
704,643,072 bytes.
SUSFS checkout allocation after cleanup is about 4.3 GB, down from 39.9 GB.
Filesystem TRIM completed; its reported range includes all unallocated virtual
blocks and is not a measure of host bytes reclaimed. The WSL VHDX was not taken
offline/compacted, so its Windows file size may remain larger than Linux usage.
The current eight-feature image remains experimental; rule/stability tests
are pending. Exact deletion plan, hashes and checks are in:
.backport-context/audit/cleanup-20261016T053954Z.


## 2026-10-16: WSL virtual-disk compaction

User explicitly authorized briefly stopping Ubuntu and compacting its VHDX to
return deleted-file space to Windows. Saved and verified a source-only backup
of all uncommitted changes on D: before stopping WSL. Terminating the distro
alone allowed a background connection to restart it, so the first attempt
stopped before any compaction. The successful retry shut down WSL and briefly
stopped WslService, attached only the registered Ubuntu disk read-only, used
DiskPart compact vdisk, detached it and restored WslService to Running.
DiskPart confirmed success and detached status; no partition or file deletion
command was used. Ubuntu restarted normally afterward.

VHDX reduced from 50,604,277,760 to 14,673,772,544 bytes during compaction
(33.46 GiB reduction); normal startup added about 32 MiB afterward. Windows
C: free space after restart was about 79.07 GiB, versus about 45.7 GiB before.
Verified 75,558 SUSFS and 75,469 Lineage source entries against saved hashes,
all repository refs and indices, uncommitted diffs, protected-branch commit
readability and active provider links. Current/recovery/original boot-image
hashes remain exact. This log entry was added after those source checks;
no kernel code changed and no phone operation occurred.

Evidence: .backport-context/audit/wsl-compaction-20261016T055418Z.
Windows source backup and complete maintenance record:
D:/Phone expriment/WSL-Maintenance/20261016T055418Z.


## 2026-10-16: Fresh LineageOS fork publication

Preserved the complete local backport snapshot and legacy Git history before
constructing source from clean LineageOS c65462bf08da2a8900b98e705bea376de7f7f19c.
All 55 reviewed production backport file hashes and the 16-file BakaSU provider
patch remain identical to the prior all-feature build. The three newer upstream
paths remain unchanged. The default lineage-23.2 branch contains SUSFS;
lineage-23.2-bakasu contains the separate pinned manual-hook integration.

Added exactly two manual-only workflows, with pinned checkout/upload actions,
verified Clang archive/executables, read-only repository permission and
kernel-only artifacts. No original ROM image, device log or credential is
published. Fresh-clone source checks no longer require the deleted fork's
legacy branch; the old anchor is still checked if present locally. Native
BakaSU compatibility/version, CFI/LTO and module checks are retained.
Regression comparisons now use the immutable clean Lineage base, while
pre-fix negative controls use explicitly hashed legacy source fixtures.
Configuration/native Kbuild preparation and drift-refusal tests pass with all
eight features. The maintained host suite covers 17 source/ABI models; old
staged-only checkpoint tests remain historical, documented separately.
This source publication is not a new device test or a runtime safety claim.


### Cloud compiler transport correction

First cloud runs 38862453540 and 38862456448 stopped before compilation at the
compiler archive SHA256 check. Two independent immutable-commit archive probes
showed different mtimes/container hashes with identical contained file hashes.
The extracted official compiler matches Git tree
17fee43bde5bc5860dc1d04be5ead3bd6e6f91f0. Installer integrity now covers the
complete Git tree and a pinned SHA256 content tree plus the seven original
executable hashes; no unverified compiler is executed. Native BakaSU checks
and all 55 kernel production hashes are unchanged. Independent Git SHA1/SHA256
comparison, timestamp-independence and content/mode/fingerprint/corrupt-archive
negative tests pass, as do the preparation/native-Kbuild regressions.
Replacement cloud runs: 38863933185 (BakaSU), 38863936189 (SUSFS). These are
currently running; no successful new cloud-build or device-test claim is made.


## 2026-10-16: Selectable kernel or Axion boot output

Cloud runs 38863933185 (BakaSU only) and 38863936189 (all-eight SUSFS)
completed successfully after the compiler content-integrity correction.
Added a required manual output_format choice to both workflows: kernel
(default) or boot.img. Raw output is copied unchanged; boot.img uses the exact
supplied Axion original and the reviewed boot-v3/unsigned-AVB repacker.
A single success-only upload contains exactly Image/Image.sha256 or
boot.img/boot.img.sha256. Internal modules, DTBs, configuration and diagnostics
remain runner-local. No device operation or new kernel compilation was run
for these packaging-only changes.

Ten packaging regressions passed using the actual supplied boot base and
a previously compiled ARM64 Image, including raw identity, exactly two files,
SHA256 format, ramdisk/header/AVB preservation, failed/corrupt/mismatched build
refusals, base archive hash/cache checks, and non-overwrite behavior. Both
workflow contracts and every shell step passed syntax checks. Native build
preparation/refusal tests pass; all 55 kernel production fingerprints match.
The user explicitly approved uploading the original build resource. The
32.2 MiB compressed base was published as the axion-2.8-boot-base prerelease
(release 414832064, asset 646986885), targeting the clean Lineage base.
GitHub's asset digest matches the pinned compressed SHA256. Newly compiled
images remain Actions artifacts; no boot image is committed to Git.

Public original-resource verification passed using unauthenticated Linux and Windows HTTPS downloads. Archive/image SHA256 pins matched. Both output formats passed with real compiled SUSFS build evidence and contained exactly two files. Temporary copies were removed. These workflow updates are manual only; no new kernel build was dispatched.
