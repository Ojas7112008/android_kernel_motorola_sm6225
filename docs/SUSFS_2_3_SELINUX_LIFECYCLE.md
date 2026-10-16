# Devon SELinux policy lifecycle checkpoint

**Historical checkpoint:** Source selection and SELinux ownership have been superseded by the [automatic-hook checkpoint](SUSFS_2_3_SELINUX_AUTOHOOKS.md). Original source pins/results below are preserved as historical evidence.

Date: 2026-10-13. This follows the approved [SELinux adapter](SUSFS_2_3_SELINUX_ADAPTER.md). The user authorized the next lifecycle review and concrete fixes. The full SUSFS-enabled build remains blocked by BakaSU's unchanged compatibility stop.

## Findings and changes

The pinned BakaSU 4.19 path formed newpolicydb from an unchecked kcalloc result and passed an unchecked backup allocation into serialization. It assigned backup globals before the candidate state was complete, and repeated initialization replaced the saved policy with a copy of the current, potentially modified policy. handle_sepolicy could reach an unconditional selinuxfs-mutex unlock after an early copy_from_user failure, before that mutex had been locked.

The authoritative [BakaSU adapter patch](../.github/patches/bakasu-devon-selinux-adapter-f443.patch), still against f4435f007b16f61705089ff982e3f981a6017979, now contains seven files: the original Kbuild, selinux_hide.c/.h and rules.c changes, plus selinux/selinux.h, runtime/ksud_integration.c and selinux/sepolicy.c. Immutable reference checkouts are unchanged. The editable preparation tree remains ignored.

For KSU_COMPAT_SUSFS_DEVON_SELINUX_ADAPTER, rules.c prepares a private replacement policy and, on the first successful installation, a private saved-policy/SID-table pair. The transaction publishes them only after the copy/initial-SID stages succeed. Every failed preparation releases its allocations and both mutexes while retaining the live policy and existing backup. Publication and destruction follow hide mutex -> selinuxfs mutex -> policy rwlock. The previous default-rule body was extracted into a helper and verified byte-identical; its rule mutation semantics are not claimed fixed by this checkpoint.

A persistent flag protected by the hide mutex records successful original-policy capture. Repeated setup retains that first snapshot. If boot completion drops an unused snapshot, subsequent setup does not recreate one from already modified policy. Policy batches require successful initial setup before starting a transaction. Their existing positive partial-success count semantics are preserved; malformed command framing discards the private candidate, as intended by the original handler. Input-copy errors release only the payload and do not unlock an unacquired mutex.

The target apply_kernelsu_rules interface returns an error. All three built-in init-exec call sites stop before cache_sid, setup_ksu_cred and the second-stage completion flag when copy/setup fails. A later matching exec can retry. Other integration modes retain their void interface and original implementation. The remaining caller is in the MODULE-only late-load path, which the devon adapter explicitly rejects. This is error propagation for private-copy/setup stages, not proof that every rule mutation or credential transition succeeds.

The target serialization path rejects null copy arguments, empty/overflowing capacities and capacities exceeding INT_MAX using checked size arithmetic. This matches the existing int length/result interface and prevents truncation before vmalloc. The configuration-field bounds comparison also now checks a nonnegative length explicitly.

## Kernel lifecycle audit

Motorola's selinux_initialized uses smp_load_acquire, paired with the initial-policy publication store. Transactions reject uninitialized state before dereferencing the mount/policy state. selinux_disable rejects disabling after the first successful policy load, so a valid initialized transaction does not cross the early disable/unmount path.

The native selinuxfs load path holds its filesystem mutex. Reload constructs a new policy and SID table, converts the live SID table, swaps both under policy_rwlock and frees the old state after readers drain. The typed adapter selects its live SID-table pointer after taking that read lock, and never treats a saved-policy SID as a live credential SID. Native reload is not intercepted, suppressed or made to auto-install BakaSU rules by this patch. Cached BakaSU SID/domain behavior across real reloads still needs review and runtime validation.

Ownership on error was checked against Motorola's actual implementations. policydb_read cleans failed policy internals itself, including policydb_init failure cleanup. policydb_load_isids destroys initialized SID-table contents on its failure paths. A transaction records successful construction before taking responsibility for internal destruction; failed constructors receive wrapper cleanup without a second internal destroy.

The status-page reference and per-open selection contract from the previous adapter remains unchanged. No fake_state, new permissive bypass, version change or build-check relaxation is introduced.

## Executed validation

Run:

```sh
python3 scripts/susfs/test_selinux_adapter.py
```

The runner reconstructs the complete seven-file patch from pinned Git blobs in a temporary Git baseline, checks strict applicability/reverse applicability and reconstructed-source whitespace, and verifies the original compatibility fragment SHA-256. It extracts actual source functions rather than copies of their implementation.

The original three adapter configurations still pass with Clang 21.1.8 and ASan/UBSan. Three additional lifecycle binaries test the Android init argv path, older init argv path and older init environment path independently. Each runs:

- All eight allocation failure positions in private policy-copy/saved-policy preparation, with unchanged live state, no backup publication, balanced allocations and no held lock afterward.
- Serializer, policy-reader and initial-SID failures; uninitialized/missing state and missing mount; first-snapshot retention and no recapture after retirement.
- Early payload/copy errors; all four update-copy allocation failures; malformed header/string rollback; successful publication and preserved positive partial-success counts.
- Null arguments, signed length limits, size overflow and zero capacity.
- Second-stage copy failure skipping SID/credential caching, successful retry and one-time completion.

All six configurations pass; runner exit 0. Their policies, rule primitives, user copies and batch framing are host stand-ins. The complete policy writer, serializer, ownership paths and init hook control flow are actual extracted functions. Tests do not validate real policy bytes, flex arrays, MLS/AV semantics, kernel locks/RCU, CFI, the ARM64 layout or device behavior.

The first lifecycle compile run exposed an inherited signed/unsigned bounds comparison and a mock logging macro that made a logged counter appear unused. The comparison and mock were corrected before the successful run. The BakaSU patch was also checked with Motorola's kernel checkpatch: exit 1, with 101 indentation errors and 306 warnings. All errors are tab-indentation complaints; BakaSU kernel/.clang-format explicitly uses IndentWidth 4 and UseTab Never. Native BakaSU indentation is retained. This is not reported as a clean kernel-style check. Strict reconstructed-source whitespace validation passes.

The Kbuild probe still identifies the adapter, then the unmodified susfs_compat.mk stops make with exit 2 at line 20. Logs and hashes are under .backport-context/audit/selinux-adapter/.

## Remaining concrete risks and next boundary

Update: the subsequent [type-creation checkpoint](SUSFS_2_3_SELINUX_TYPE_CREATION.md) implements the focused add_type fix and critical setup error propagation described below. The following paragraph records the finding at this lifecycle checkpoint; broader mutation and kernel/runtime validation remain outstanding.

The lifecycle review found a separate important mutation-primitive problem: add_type increments p_types.nprim and inserts type/key data before all replacement flex arrays and bitmap operations succeed. Failures can leak or leave inconsistent private policy structures. Its old-data copy bounds and ignored allocation/bitmap results require a focused fix and fault tests against the actual Motorola flex-array layout. The default-rule body still ignores primitive return values, and the existing userspace batch intentionally permits partial success. Private-copy fault tests do not certify these mutation-failure paths.

Other outstanding checks include real SID-cache/credential readiness across reload, concurrent native reload versus batch/update/query/toggle operations, actual callback atomic-context rules, policy load sequencing, real constructor cleanup under fault injection, secondary-user/isolated UID handling and status descriptor/mapping stress. The environment-detection loop also starts at argument index 1; its environment indexing belongs in the upcoming exec/init integration audit and was not silently changed here.

The next source checkpoint should fix and test the target's type-creation allocation contract before claiming safe policy installation. Then audit SID/credential readiness and finish the remaining BakaSU hook interfaces. Integrated ARM64 compilation still requires a legitimate compatible build route; the upstream stop must remain effective until that exists.

No full kernel compile/link, image operation, push, CI dispatch, boot, flash or device test was performed. The original checkout and protected devon-resukisu ref remain unchanged.
