# Devon SELinux adapter checkpoint

**Historical checkpoint:** Source selection and SELinux ownership have been superseded by the [automatic-hook checkpoint](SUSFS_2_3_SELINUX_AUTOHOOKS.md). Original source pins/results below are preserved as historical evidence.

Date: 2026-10-13. The user authorized adapting the SELinux interfaces on the isolated susfs-2.3-backport branch. This is a source/host-contract checkpoint, not a supported SUSFS-enabled kernel build.

## Sources and deliverables

- Motorola kernel base: 18bd9752ada79090ce11326662eaaa9f33717728, Linux 4.19.325.
- BakaSU provider patch base: f4435f007b16f61705089ff982e3f981a6017979.
- SUSFS reference: 82f08351fadb8b19dd6591bd14634d02ad832145, version 2.3.0.
- Kernel interface: [preserved kernel interface](../scripts/susfs/fixtures/f443-kernel-selinux.patch).
- Kernel consumers: [hooks.c](../security/selinux/hooks.c) and [selinuxfs.c](../security/selinux/selinuxfs.c).
- External provider integration: [bakasu-devon-selinux-adapter-f443.patch](../.github/patches/bakasu-devon-selinux-adapter-f443.patch), against the exact BakaSU pin above.
- Host verification: [test_selinux_adapter.py](../scripts/susfs/test_selinux_adapter.py) and its [C fixture](../scripts/susfs/test_selinux_adapter.c).

The BakaSU and SUSFS reference checkouts remain source-clean. The initial four-file editable BakaSU preparation tree is ignored under .backport-context/work/bakasu-selinux/. The tracked patch is authoritative for these external changes; tests reconstruct it from the pinned Git blobs rather than trusting the editable preparation tree. The subsequent [policy lifecycle checkpoint](SUSFS_2_3_SELINUX_LIFECYCLE.md) expands the provider patch to seven files and adds publication/copy/boot error tests. The [type-creation checkpoint](SUSFS_2_3_SELINUX_TYPE_CREATION.md) subsequently adds native-primitive allocation-failure tests and critical setup error propagation.

The kernel changes are direct source edits. Do not apply the old 1.5.9 kernel patch over the eventual 2.3.0 implementation. The legacy patch remains unchanged and still passes a strict applicability check at this checkpoint. The existing workflow has not been updated: it still selects the old integration and does not apply this new BakaSU provider patch. It is not a validation route for this adapter.

## Interface and behavior

| Operation | Handled path | Ordinary path / error contract |
|---|---|---|
| set_current_sid | Parse against saved and live policies, reject either error, return only the live SID | UID below 10000 or hide inactive returns false; active missing backup returns handled -EAGAIN |
| context_to_string | Paired parser and saved-policy canonicalization within one protected operation | Preserve BakaSU query semantics: saved validation determines the query result; caller owns the allocated string |
| compute_av | Resolve both contexts and compute against the same protected saved policy | Permission check and transaction formatting remain in Motorola selinuxfs; no saved SID escapes |
| get_status_page | Return a saved status page with one acquired reference | NULL selects the real page; kernel caller acquires a reference to that page instead |

The header's SUSFS_SELINUX_ADAPTER_ABI value is an interface revision, not SUSFS_VERSION or a feature-completion claim. No SUSFS version number changed. All calls are under CONFIG_KSU_SUSFS. Built-in CONFIG_KSU is required; incomplete/module linkage is rejected rather than supplied with stubs. The provider also rejects layouts outside the audited Linux 4.19 selinux_state.ss/pointer-sidtab model.

The kernel context hook applies only to the current attribute, after its original permission check and newline handling. Motorola's prepare_creds, zero-SID rejection, bounded transition, DYNTRANSITION, ptrace checks and commit/abort paths remain in place. Other attributes use their original converters and checks. A handled validation failure cannot silently fall back to the ordinary converter.

The query hooks run after the original live-policy CHECK_CONTEXT or COMPUTE_AV permission check. Request parsing, allocation, transaction bounds, response formatting and cleanup remain in selinuxfs. The UID threshold intentionally matches the pinned BakaSU behavior; classification of secondary-user/system/isolated UIDs remains a runtime review item.

The provider patch adds an explicit Kbuild interface probe. A matching kernel selects the typed provider and suppresses the old automatic SELinux interception, avoiding duplicate callbacks. This does not declare the complete SUSFS/BakaSU pairing supported. kernel/tools/susfs_compat.mk is unchanged byte-for-byte, including its unconditional stop. The probe is reached only in the existing SUSFS integration branch.

## Ownership and synchronization

Saved-policy readers, feature toggles, backup publication in apply_kernelsu_rules and backup destruction share selinux_hide_mutex. Readers keep it through parsing plus canonicalization or AV computation. Publication follows the order hide mutex -> selinuxfs mutex -> policy rwlock. The paired legacy parser obtains the live policy/SID-table pointers after taking the live policy read lock; a pre-lock sidtab pointer is no longer passed through a reload window in this adapter mode.

The paired parser now initializes SID/error outputs on early exits, assigns a live SID only after successful insertion, and destroys a successfully parsed live context even if saved-policy validation fails. These cleanup/output fixes are in the shared parser; the executed tests exercise its legacy 4.19 branch. The underlying user/role/type/MLS parsing and both-policy validity checks are retained. Motorola has the backported char * MLS interface; BakaSU's existing stricter-MLS compatibility probe remains responsible for selecting it.

Status open retains one page reference for both saved and real pages. Release drops it. A mapped VMA retains its file, so closing the descriptor does not retire the page while that mapping retains the file. Selection remains per open. The existing one-page, offset-zero, read-only mapping checks are retained. BakaSU's existing __free_page retires its owner reference; Motorola __free_pages uses put_page_testzero, so an open file's extra reference prevents immediate freeing. No dummy fake_state is introduced and no policy object is exported through the interface.

## Executed validation

Run locally from the isolated kernel worktree:

```sh
python3 scripts/susfs/test_selinux_adapter.py
```

The runner verifies the BakaSU pin and source-clean reference; strictly applies the tracked patch to a temporary Git baseline; reverse-checks it; checks whitespace in the reconstructed source; extracts the actual callers, paired parser, adapter, feature setter, exit and backup-destruction functions; and compiles host mocks with Clang 21.1.8, ASan/UBSan and incompatible-pointer diagnostics as errors.

Passed configurations: SUSFS off, SUSFS on without static keys, and SUSFS on with static keys. Tests cover ordinary fallback, unrelated proc attributes, permission denial before parsing, live credential SID selection, both-policy rejection and error precedence, both string-allocation failures, SID insertion failures, context cleanup, selection after a simulated policy reload, canonicalization failure, malformed requests, status read/reference retirement, mmap restrictions and a concurrent reader versus the actual feature-disable/backup-destruction functions. A module/incomplete header configuration fails as expected.

The actual Kbuild adapter probe was separately evaluated, then the unchanged upstream compatibility fragment was included. The adapter flags are detected and make exits 2 at susfs_compat.mk:20. This is an expected negative result and confirms the build stop remains effective, not a passing build.

Kernel checkpatch reports zero errors and one generic new-file/MAINTAINERS warning. No upstream maintainer ownership entry is invented for this local integration. Initial fixture runs exposed missing mock declarations and an incorrect expected ordinary canonical-string length; the fixture was corrected and the three configurations then passed. These were host-fixture failures, not kernel build failures.

Logs, source hashes and machine-readable outcomes are under .backport-context/audit/selinux-adapter/. Unified patch payloads contain blank context lines; check their reconstructed source with strict git apply and git diff --check. Do not interpret the payload's context spaces as new C whitespace. BACKPORT_HISTORY.md retains the already documented original Markdown hard breaks.

## Remaining gates

- The full SUSFS-enabled build is still blocked by the upstream BakaSU restriction. This patch neither removes nor bypasses it and does not complete the other command, task-state, mount or filesystem interfaces.
- Host mocks test API/control-flow/ownership contracts. They do not validate real policy parsing, SELinux policy semantics, memory ordering, spinlock/atomic-context rules, lockdep, RCU, kernel mmap lifetime or timing behavior.
- Review all full-tree policy load/update/initialization paths and allocation/error handling, including existing apply_kernelsu_rules failure paths and repeated initialization. Run kernel fault injection and concurrent reload/toggle/query stress once a legitimate supported build route exists. The original host race test covers reader versus disable/destruction. The [lifecycle tests](SUSFS_2_3_SELINUX_LIFECYCLE.md) now execute the policy-publication function with mocked primitives; real mutation internals and kernel concurrency remain unvalidated.
- Build the actual ARM64 objects and final kernel through unmodified compatibility checks, with the selected Android toolchain and production CFI/LTO configuration. Verify actual feature guards, object providers and symbol linkage.
- Later approved runtime validation must cover genuine policy reloads, MLS contexts, denied and allowed transitions, ptrace, multi-threaded callers, ordinary/root/isolated/multiple-user contexts, status descriptors passed between processes and mappings held across toggle/teardown. Compare permission behavior against the protected baseline.

No kernel/ARM64 compile or link, image production, push, CI dispatch, boot, flashing or device runtime validation was performed at this checkpoint.
