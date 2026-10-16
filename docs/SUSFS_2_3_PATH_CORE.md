# SUSFS 2.3.0 path core checkpoint

Date: 2026-10-14 (Asia/Calcutta). Isolated source/host validation and request layout probes; production feature consumers and kernel build remain incomplete.

The user authorized the next major stage. This implements the path-core portion of the remaining feature-core stage after the [core foundation](SUSFS_2_3_CORE_FOUNDATION.md). Eight prior source/test/pin/result inputs and the absence of the path source are preserved under .backport-context/audit/path-core/prior-checkpoint. Source pins and the 16-file external BakaSU provider remain unchanged; current limits are in [source-pins.json](../scripts/susfs/source-pins.json).

## Source and production boundary

[fs/susfs_path.c](../fs/susfs_path.c) provides real add-path/add-loop handlers, deferred iteration, the task/owner/inode predicate, the pinned fake qstr and an owned /data path lookup. [fs/Makefile](../fs/Makefile) selects this object with CONFIG_KSU_SUSFS_SUS_PATH. The [public header](../include/linux/susfs.h) adds only typed declarations and the shared work object; removing that exact declaration block reproduces the pinned 2.3.0 header byte-for-byte. Version, constants, structures and payload layouts are unchanged.

The aggregate core deliberately still rejects CONFIG_KSU_SUSFS_SUS_PATH. Its lookup, dcache and readdir/getdents consumers have not been ported, and its capability query does not advertise SUS_PATH. The new tests compile the whole isolated path object plus actual core work/command functions and pinned transport/dispatch; they do not remove or bypass the aggregate/upstream checks to obtain a complete kernel. The whole-core runner still exercises the actual production rejection. This is a path-core checkpoint, not a completed hiding feature or the entire first major stage.

## Command and pathname contracts

The pinned void(void-user-pointer-reference) handlers retain their payload error-field replies and real-UID-zero transport gate. Input is copied before any lookup, allocation or registration. Empty names fail with EINVAL; a buffer without a NUL in its 256-byte pathname fails with ENAMETOOLONG. All 255 valid pathname bytes are retained. Input strings remain immutable during the operation.

Immediate marking uses LOOKUP_FOLLOW, the caller's credentials, namespace and CWD, including relative names. It returns real lookup errors and drops the held path on every success/error path after lookup. Missing inode/mapping fails with ENOENT. It sets only AS_FLAGS_SUS_PATH (bit 33), preserving the native mapping flags. The vendor's ordinary mapping bits are below 33; 64-bit storage is required by the shared target guard. No native inode/address-space layout changes are made.

FUSE embeds the same VFS inode returned by get_fuse_inode. Marking its public i_mapping once preserves the same bit semantics without casting arbitrary inodes into a filesystem-private type or importing FUSE internals. Tests inspect/execute the vendor's container helper and check the embedded-inode identity; real FUSE IO and daemon behavior remain unvalidated.

Deferred requests require absolute names, the initial mount namespace and a root path equal to init_task's full root. Private namespaces/chroots fail with EXDEV rather than being silently reinterpreted by the worker. The initial namespace must be initialized. Root snapshots acquire/release native path references, and no caller namespace/root reference is retained in the registry. This is a deliberate stricter port contract than upstream's unqualified strings; target daemon/module callers must be checked before production enablement.

Entries are deduplicated by their exact pathname bytes under the writer mutex. Existing entries remain successful without allocation, including at capacity or during an allocation fault. At most 1024 unique immutable entries are retained until reboot, bounding both registry growth and one-pass traversal. New entries beyond that limit fail with E2BIG; allocation failures return ENOMEM without publication. The limit is an implementation bound, not a changed command number or payload.

A successful ready registration also queues the existing statically initialized extra work; duplicate requests can trigger another retry. A loop registration does not require the target to exist and reports storage success, not proof of marking. The pinned BakaSU Rust CLI separately checks existence before sending its request. A failed reply copy can leave the old userspace sentinel while marking/registration has occurred, matching the existing void-handler transport convention.

## SRCU and task semantics

Publication uses the vendor's list_add_tail_rcu under the writer mutex. Readers name the path SRCU domain explicitly with srcu_dereference and keep it held across potentially sleeping lookups. There is no writer mutex held across VFS IO. Entries never mutate or disappear after publication and have no deletion/unload API; they intentionally live until reboot in the built-in target. No grace-period wait or cleanup is introduced into a reader.

The worker requires acquire-published KSU credential readiness, a non-null KSU credential, and the same initial namespace/root context. It overrides/reverts that immutable credential for each pass. Per-entry lookup failures do not discard registrations; later UID-triggered work or ready registrations retry them. A retry resolves names again and can flag a replacement/reclaimed inode. There is no new periodic path-loop timer. Kernel workqueue mount/root context and real KSU policy permissions remain device validation requirements.

The inode predicate returns true only for a marked live mapping in an umounted process with UID >= 10000 whose UID differs from the inode owner, preserving the pinned owner exception. Null inode/mapping, root/system UID, unmarked mappings and unclassified tasks remain visible. It uses typed kuid equality rather than interpreting FUSE-private owner fields. The /data helper returns an owned path on success; its caller must put that path. No native hiding caller uses these helpers yet.

## Executed validation

[test_susfs_path.py](../scripts/susfs/test_susfs_path.py) and [test_susfs_path.c](../scripts/susfs/test_susfs_path.c) compile the whole path object with real header contracts, pinned command transport/dispatch and the actual core extra-work callback. They extract vendor SRCU pointer macros, RCU list publication/initialization, root/path helpers and the FUSE container helper. VFS, credentials, user copies, work scheduling, SRCU counters and locks are host models; native SRCU grace periods and real syscall/filesystem behavior are not executed.

Both BakaSU static-key compatibility modes pass under Clang 21.1.8 and ASan/UBSan, with undefined-behavior reports configured to halt the process. Checks cover copy faults, root gating, invalid/relative/maximum names, caller permissions, missing inode/mapping, native bit preservation, owner and multi-user task predicates, owned path balance, readiness and credential restore, private namespace/chroot rejection, exact full-length storage, capacity, allocation failure, duplicates and replacement retries. A writer publishes while a reader is deliberately blocked in lookup; four concurrent writers/readers exercise duplicate publication and bounded traversal. Test-only reclamation runs after readers join so the harness itself leaks no permanent-registry entries.

The actual pinned add-loop function's negative control accepts a 255-byte name but stores a different 254-byte prefix because it ignores the bounded copy result. The new code stores the complete valid name. Wrong legacy payload pointer and untyped inode predicate fail compilation; absent SRCU fails the actual object guard. The unchanged aggregate rejects SUS_PATH and does not list it as enabled.

Freestanding aarch64-linux-android and armv7a-linux-androideabi probes verify the unchanged path payload is 260 bytes, with err at offset 256, against the exact JNI declaration and command values. Rust repr(C)/array/i32 fields are inspected. Together with the previous five core payloads these are six fixed-layout request/query structures; native-long feature ABI and real ARM32 transport remain pending.

Evidence is under .backport-context/audit/path-core. Final input/ref preservation, regressions and style results are recorded in [PORT_LOG.md](PORT_LOG.md). Initial harness fixes imported the vendor list_next_rcu/INIT_LIST_HEAD definitions and negative-control logging model. Source review explicitly uses the real SRCU macro domain; it does not assume generic older-kernel list behavior over the vendor's backported headers.

## Next boundary

Port and validate the corresponding lookup/dcache and readdir/getdents consumers, including RCU/ref-walk retry, negative dentries, user-copy/offset/error behavior and Motorola inode_permission2 semantics. Only remove the production gate and advertise SUS_PATH after those consumers pass. Then continue other feature core/payloads and mount-ID/namespace ownership, followed by the pinned workflow and actual ARM64 kernel builds. No image, flash or runtime safety claim is made here.

Error-field replies now check unsigned address addition before copy-out, including the shared core error helper. Near-maximum malformed user pointers are exercised with the actual vendor overflow helper/macro, avoiding wrapped field addresses and pointer-overflow sanitizer faults. Payload conventions and production feature gates remain unchanged.

Final audit: 31 current host configurations have passing recorded results; affected path/core/automatic SELinux suites were run for this checkpoint and all other input hashes still match. All 125 immutable inputs, original/protected state, eight archived inputs, unchanged external provider, 17 native files and 103 local document links are verified. The new stage diff has zero checkpatch errors and one new-file MAINTAINERS warning. Evidence is .backport-context/audit/path-core/final-integrity.json.

Later checkpoint: [initial path consumers](SUSFS_2_3_PATH_CONSUMERS.md) now implements guarded lookup/cache and directory emitters. The descriptions above record the earlier path-core checkpoint; the aggregate gate remains closed for the newer documented enumeration/fallback coverage gaps.
