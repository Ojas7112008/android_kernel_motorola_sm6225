# SUSFS 2.3.0 open/create handoff and overlay identity

Date: 2026-10-15 (Asia/Calcutta). Guarded source and host validation only. Production SUS_PATH enablement and ARM64/device validation remain pending.

Follow-up: [merged-overlay enumeration and shared-cache isolation](SUSFS_2_3_OVERLAY_MERGED.md) now covers successfully resolved merged entries. Direct backing-directory misses and native validation remain open. Details below describe the preceding checkpoint.

This follows the [live FUSE index checkpoint](SUSFS_2_3_FUSE_PATH_INDEX.md). The user resumed the SUSFS work after the separate non-SUSFS LineageOS/BakaSU build and repository consolidation. Twenty-four preceding inputs/results and the full preceding diff are preserved under .backport-context/audit/path-open-create/prior-checkpoint. The SUSFS checkout now has an independent Git directory; the removed original checkout is not a prerequisite. The protected devon-resukisu reference remains unchanged.

## Open/create handoff

The previous lookup_open guard inspected the inode before the filesystem's create callback. A successful callback can instantiate a different, marked inode or publish a marker during creation. Native do_last can then take its FMODE_CREATED route to finish_open_created, skipping the normal walk checks. The callback result therefore needed its own check before the path handoff.

[fs/namei.c](../fs/namei.c) now rechecks d_backing_inode(dentry) at out_no_open, before assigning the output path. A hidden result returns ENOENT through the native dput cleanup. Filesystem errors retain priority; successful creation still emits its native fsnotify notification. The native FMODE_CREATED error convention, permissions and atomic-open interfaces are preserved. This does not roll back a completed create or its remote side effects, revoke an already open descriptor, or establish an atomic policy guarantee against concurrent marker/owner changes.

The [consumer fixture](../scripts/susfs/test_path_consumers.py) now executes the actual native open_to_namei_flags and atomic_open helpers as well as lookup_open. Filesystem callbacks and surrounding VFS services remain host models. Tests cover negative cached and cold dentries, aliases, create/exclusive/truncate flags, a returned marked inode, publication during create, task/owner exceptions, native callback errors, notification behavior and reference cleanup. A native atomic-open permission failure retains the file reference until the modeled caller cleans it up.

## Overlay identity boundary

In this exact vendor tree, d_backing_inode returns the view inode; d_real_inode can resolve a physical backing inode instead. Native inode initialization gives each view its own mapping. Overlay copy-up and attribute/flag updates do not replace that mapping. Marking a physical backing inode would affect other views and would not preserve the intended view-local identity contract.

The new [overlay runner](../scripts/susfs/test_path_overlay.py) and [fixture](../scripts/susfs/test_path_overlay.c) execute native dentry/backing/real helpers, overlay inode accessors, copyattr/copyflags, initialization, publication and d_real. Constants and the overlay flag enum are extracted from the actual headers. Cases include lower-only, copied-up and metadata-only upper views, nested views, a shared physical backing alias, owner/task gates and balanced modeled references. Native overlay/dcache/inode sources remain unchanged. Surrounding private structures, locks/barriers, reference operations and hash insertion are models: these tests do not exercise a mounted overlay, native copy-up concurrency or kernel RCU.

Native ovl_iget5 hashes a backing-inode pointer, whereas directory entries can report a real inode number or a getattr/xino-derived number. Generic ilookup(sb, emitted_ino) does not establish an overlay identity contract. [fs/readdir.c](../fs/readdir.c) now explicitly skips that unsupported fallback on overlayfs after the existing named-child check. A cached marked child is filtered; a cold entry remains visible. This is a documented coverage gap, not cache-independent overlay hiding. The FUSE reported-inode index remains unchanged and its regressions were rerun.

## Validation and evidence

Commands run from the repository root, all exiting successfully:

```sh
python3 -B scripts/susfs/test_path_consumers.py
python3 -B scripts/susfs/test_path_overlay.py
python3 -B scripts/susfs/test_fuse_path_index.py
python3 -B scripts/susfs/test_access_stat.py
```

These exercise 14 positive host configurations: four consumer PATH/compat combinations, two overlay PATH combinations, four FUSE BPF/DAX combinations and four access/stat hook modes. Clang 21.1.8 runs with ASan/UBSan and fatal undefined behavior. Together with unchanged earlier reports there are 41 recorded positive configurations; the final audit checks their source input hashes.

Four consumer mutations must fail: bypassing the pre-atomic filter, removing the new post-create guard, changing a hidden directory record into a buffer error, and bypassing the overlay-number guard. The overlay mutation substitutes d_real_inode for d_backing_inode and must fail. Existing FUSE lifecycle/identity negative controls still pass.

Fixture development initially exposed missing native flag-helper extraction, an invented fsnotify positive-dentry assertion, an incorrect assumption about the initializer's local mapping variable, a missing unlikely model and a shared WARN_ON definition. Those were corrected; the FUSE fixture explicitly undefines the shared macro before its stricter assertion. Representative flag values were replaced by extracted native definitions. These were fixture/source-extraction issues, not target kernel compiler failures.

The incremental kernel patch passes checkpatch with zero errors and zero warnings (24 lines checked). This does not supersede the older provider/header style findings. Removing the guarded consumer additions still reproduces native namei/readdir. Source whitespace, local document links, all 125 immutable inputs, preserved checkpoint inputs, clean exact source pins and the protected reference are checked in .backport-context/audit/path-open-create/final-integrity.json. The authoritative 16-file BakaSU provider, source-pins.json, version declarations and production rejection/capability logic remain unchanged.

The final audit resolves 137 local document links. Source whitespace passes after excluding the byte-preserved handoff and three raw patch artifacts; the full raw scan's existing context/hard-break formatting is retained in whitespace-raw.log. In the separate non-SUSFS checkout, all 20 original kernel-only artifact entries match their saved sizes/hashes. The later boot-build directory named in the prior handoff, artifacts/boot-builds/20261015T061604Z-0yqw9x, is absent locally. Its boot-image output cannot be reverified from that location. No separate-project file was edited or output recreated here.

## Remaining work

1. Design and validate cache-independent overlay enumeration identity and lifetime, including copy-up, xino, aliases and duplicate reported numbers. Review generic inode-number fallback on other supported filesystems.
2. Resolve remote-create side effects and concurrent name/mount/owner/marker publication semantics with real native locking/RCU evidence. Measure the FUSE index and validate remaining reported-metadata/duplicate assumptions.
3. Complete native integration checks before reviewing production SUS_PATH enablement and truthful capability output. The aggregate rejection remains in place.
4. Continue the remaining feature bodies/ABI and mount-ID/namespace work, then pin the SUSFS ARM64 workflow/configuration/toolchain and attempt target builds. Boot and runtime testing require separate authorization.

The separate non-SUSFS kernel build does not validate this SUSFS source. This checkpoint creates no SUSFS image and performs no push, CI dispatch, release, flash or device operation.
