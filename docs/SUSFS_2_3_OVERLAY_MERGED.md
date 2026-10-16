# SUSFS 2.3.0 merged-overlay enumeration checkpoint

Date: 2026-10-15 (Asia/Calcutta). Guarded source and host tests only. Production SUS_PATH, target linking, boot and runtime validation remain pending.

This follows the [open/create and overlay identity review](SUSFS_2_3_OPEN_CREATE_OVERLAY.md). The user authorized the next step. Twenty-nine prior inputs/results and the complete preceding diff are preserved under .backport-context/audit/overlay-merged/prior-checkpoint. All immutable handoff/source inputs, source pins, the authoritative BakaSU provider and the protected working-branch reference remain unchanged.

## Problem and resulting behavior

This vendor overlay implementation has distinct merged-cache, direct-backing and inode-number-translation paths. An emitted inode number does not identify an overlay view. The previous generic emitter therefore deliberately left a cold overlay entry visible. Native merged iteration already holds the overlay directory lock outside the backing directory's iterator; it can resolve the entry by its actual name without inventing a numeric identity.

[fs/overlayfs/readdir.c](../fs/overlayfs/readdir.c) now checks merged entries before inode-number translation and emission. For a filtered app, it resolves the actual view using the native lookup machinery and applies the existing inode/owner policy. A hidden entry advances the existing cursor/cookie just like a whiteout. Its cache entry is not removed, relabeled or assigned a fake inode number. Dot/dotdot and unfiltered callers skip this policy lookup. A lookup failure leaves the probe undecided and preserves the native iteration/error path.

The resolved dentry pins the view inode through the policy check and is released once. This handles cache misses and native alias returns when the overlay lookup resolves the marked view. It does not preserve an inode marker across eviction/replacement or force distinct unhashed native views to share a marker.

## Keep shared inode-number translation independent of the caller

Native ovl_cache_update_ino uses lookup_one_len while building or refreshing shared cached inode numbers, including impure upper directories. The existing general SUSFS lookup guards could turn a marked overlay view into ENOENT during that internal lookup. Merely skipping a hidden merged record would leave the impure-cache path exposed to this error, or tempt an incorrect per-caller mutation of the shared cache.

[fs/namei.c](../fs/namei.c) adds a narrowly scoped susfs_lookup_one_len_metadata helper, declared under the feature guard in [include/linux/namei.h](../include/linux/namei.h). It accepts an overlay parent with the native required parent lock and retains native component validation, hashing, search permission, revalidation, allocation, alias/error handling and reference ownership. Non-overlay parents are rejected with EXDEV. A private local bit suppresses only the added SUSFS inode filter in lookup_dcache/__lookup_slow for this internal operation. Both functions remove that bit before passing flags to filesystem lookup or revalidation callbacks. No userspace ABI or native lookup-flag definition is added.

Only the internal ovl_cache_update_ino lookup and the new merged visibility probe use this helper. Ordinary path lookup still rejects the marked view. Shared cached inode-number translation sees the real object; the per-caller decision happens at emission and is never cached. Creator credentials, current credentials and task flags are not changed by the helper. Existing BakaSU checks are unchanged.

The metadata helper and the existing inode predicate have GPL exports, allowing an overlayfs module to reference the built-in path core. [fs/susfs_path.c](../fs/susfs_path.c) changes only by including export.h and exporting the existing predicate; its policy body is unchanged. Actual header declarations pass built-in and MODULE caller syntax probes, and a wrong helper prototype fails compilation. These are source/syntax checks, not target modpost or link results.

## Validation

```sh
python3 -B scripts/susfs/test_overlay_readdir.py
python3 -B scripts/susfs/test_path_consumers.py
python3 -B scripts/susfs/test_fuse_path_index.py
python3 -B scripts/susfs/test_access_stat.py
python3 -B scripts/susfs/test_susfs_path.py
python3 -B scripts/susfs/test_path_overlay.py
```

All commands exit zero. Twenty positive configurations were exercised: four merged-overlay PATH/compat modes, four general consumers, four FUSE BPF/DAX modes, four access/stat modes, two path-core static-key modes and two overlay-identity modes. There are 45 current recorded positive configurations overall, plus the separately retained historical adapter reports. Unaffected reports are accepted only after their input hashes match. Host compiler: Clang 21.1.8, ASan/UBSan with fatal undefined behavior.

The new [runner](../scripts/susfs/test_overlay_readdir.py) and [fixture](../scripts/susfs/test_overlay_readdir.c) execute actual lookup helpers, merged iteration, cache inode-number calculation, impure-cache processing, cursor seeking, dir_emit and native/compat record consumers. Native private directory structs, list operations, overlay path-type constants and STATX_INO are extracted. Filesystem callbacks, cache population/invalidation, getattr results, rb-tree insertion, locks and IO remain explicit host models.

Coverage includes cold/cached/alias results, regular lookup denial versus internal metadata success, invalid components, hashing/permission/revalidation/allocation/native errors, negative dentries, owner/root/task exceptions, xino remapping and origin getattr, cache reuse by an unfiltered caller, marker changes between reads, whiteouts/dot entries, buffer-full retry, EOF without writes, native/compat records and compat overflow, user-copy faults and balanced references. Direct backing iteration is deliberately checked as an unchanged delegation boundary.

Four mutations must fail: restore ordinary filtered lookup inside cache translation, remove the merged emission check, leak the private bit to native callbacks, or skip native component/permission validation. Existing consumer/FUSE/path-core/identity/access negatives also pass. Initial development repairs corrected guard-adjacent blank lines for exact source restoration, extraction of a u64-return/blank-line-signature function, a native list macro dependency, the generated model include path and caller-probe string/thread helper declarations. Those were source-restoration/host-fixture issues, not ARM64 kernel compiler failures.

The incremental kernel diff, whitespace and local links are checked in .backport-context/audit/overlay-merged/final-integrity.json. Removing the guarded additions restores the original overlay readdir and namei header exactly; the existing consumer test also restores the preserved native namei/readdir source. Imported provider/header formatting findings remain historical evidence. No complete-tree style-clean claim is made.

Final audit: 125 immutable inputs, 29 preserved checkpoint inputs, three exact clean source pins, 45 current positive host configurations and 147 local document links pass. The 172-line incremental kernel diff has zero checkpatch errors and warnings. Source whitespace passes after excluding the unchanged handoff and three raw patch artifacts; their existing formatting is retained. The separate non-SUSFS integration diff is unchanged from the preceding checkpoint.

## Remaining boundary

Direct backing-directory callbacks run while the backing directory is locked. Calling the new lookup helper there could acquire that same directory through overlay lookup and deadlock. No such lookup or forced change of native real/merged directory mode is introduced. Cold direct/translated backing entries therefore retain the earlier visibility gap, including xino and impure direct enumeration after shared-cache generation succeeds. A separate identity/lifetime solution is still required there.

Merged filtering also requires a successful per-entry lookup. Readable but unsearchable directories, transient/remote lookup errors, nested layers already filtered by their own policy, and concurrent rename/copy-up/owner/marker changes remain review cases. The new per-entry lookup can cause filesystem IO and allocate dentries; native latency, lockdep/RCU behavior and large-directory cost have not been measured. Host model locks do not validate kernel lock ordering.

Next address the direct backing-directory identity/lifetime boundary without lookup recursion or caller-specific shared-cache state, then review the other filesystem fallback contracts and native concurrency/performance. Only after those gates should production SUS_PATH rejection and capability output be reviewed. Remaining feature bodies/ABI, mount IDs/namespaces, a pinned SUSFS ARM64 workflow/build and separately approved device validation still follow.

No version numbers, BakaSU compatibility checks, production feature rejection/capabilities, workflows or protected working branch changed. No SUSFS image, push, release, CI dispatch, flash or device operation occurred.
