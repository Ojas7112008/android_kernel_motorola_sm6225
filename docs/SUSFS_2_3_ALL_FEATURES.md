# Experimental eight-feature devon port

Latest device checkpoint (2026-10-16): the native ABI repair allows one
successful temporary boot with all eight options enabled, plus limited mount-ID
checks. See [SUS_MOUNT ABI repair](SUSFS_2_3_MOUNT_ABI_FIX.md) and PORT_LOG.md
for exact evidence and remaining runtime limits.


Date: 2026-10-15. Work remains on `susfs-2.3-backport`, based on
`18bd9752ada79090ce11326662eaaa9f33717728`. The user authorized implementing
all eight optional features and running local builds. No boot image, device
operation, push, release or CI dispatch is included.

This checkpoint supersedes the feature-incomplete status of the earlier
[core-only build](SUSFS_2_3_FIRST_ARM64_BUILD.md). Historical documents and
results describe their own source revisions. The original successful Image
and the entire preceding source diff remain preserved.

| Feature | Implementation and focused validation |
| --- | --- |
| SUS_PATH | Existing path/FUSE registration and native consumers, plus deferred overlay lookup, retained directory records and correct seek/retry handling. Four overlay modes and seven mutation controls. |
| SUS_MOUNT | Separate hidden class and presentation IDs, shared IDA-slot ownership for namespace copies, native clone/unwind callbacks, proc/statfs/fdinfo/handle/statx consumers. Four SMP/feature host modes and five mutation controls. |
| SUS_KSTAT | Immutable RCU metadata rules, selected stat fields, statfs snapshots, maps/inotify/fdinfo, explicit ARM32 conversion. Four kstat/mount combinations, with native and compat commands inside enabled modes. |
| SPOOF_UNAME | Bounded release/version publication under a mutex; native UTS snapshot and syscall error handling retained. |
| HIDE_KSU_SUSFS_SYMBOLS | Filtering in the proc kallsyms iterator, including compiler prefixes. Internal lookup and module symbol handling retain their native behavior. |
| SPOOF_CMDLINE_OR_BOOTCONFIG | Bounded command-line storage and proc output under a sleeping lock. This 4.19 kernel uses cmdline; no modern bootconfig API is invented. |
| OPEN_REDIRECT | Immutable forward/reverse rule publication, all five UID schemes, one owned-name native open retry, proc/readlink/maps views. Host rule, ownership, permission-error and truncation checks. |
| SUS_MAP | Maps/smaps/rollup/NUMA/map_files filtering, page-by-page pagemap zero records and remote-access boundaries. Four feature/IO modes and four mutation controls. |

All eight now have conditional implementations and capability reporting.
There is no remaining local unfinished-feature aggregate rejection. The
checked built-in KSU route, FSNOTIFY/SRCU prerequisites, upstream compatibility
checks and real SUSFS version are retained. The separate unsupported AVC-log
toggle still returns `EOPNOTSUPP`; it is outside these eight options.

## Source and build selection

BakaSU remains pinned to `e1682a2831142e68fa30bab97d5690cf90b75119`, with the
same 16-file provider patch, SHA256
`d75748e5d0a054d7ac49e56b7218f1d44ec4c6ed7ed091088469c40f7e78493c`.
Its history-derived version, policy readiness checks and compatibility guards
are unchanged. The pristine SUSFS reference remains
`82f08351fadb8b19dd6591bd14634d02ad832145`.

[build-pins.json](../scripts/susfs/build-pins.json) now records 55 reviewed
kernel/configuration files and explicit `core`, `all` and eight individual
profiles. The core fragment remains byte-preserved with all eight options
off. The driver applies each reviewed profile before olddefconfig, verifies
the resulting configuration and exact native Kbuild object selection, and
rejects unknown features or source/provider/compiler drift. Source checks
also run after compilation. Every evidence directory includes all reviewed
source files, including newly added files a Git diff might omit.

```sh
cd ~/projects/android_kernel_motorola_sm6225-susfs-2.3-backport
./build-susfs-test.sh --prepare --profile all
./build-susfs-test.sh --build --profile all --jobs 4
./build-susfs-test.sh --build --profile core --jobs 4
```

The default remains `--prepare --profile core`. Other profile names are
`sus-path`, `sus-mount`, `sus-kstat`, `spoof-uname`,
`hide-ksu-susfs-symbols`, `spoof-cmdline-or-bootconfig`, `open-redirect` and
`sus-map`. Individual configuration/Kbuild preparation passes for all eight;
this does not claim eight individual full Image links.

Feature outputs and evidence use `out/susfs-features-test/` and
`artifacts/susfs-features-test/`. Core outputs retain their original separate
directories. The manual-only workflow remains the core-profile workflow and
has not been dispatched.

All-feature run `20261015T124521Z-11i5pjlo` compiled and linked successfully
with CFI/LTO, producing a 37,521,432-byte ARM64 Image, 66 AArch64 modules and
four devon device-tree files. Image SHA256:
`d346326d668b8962b8e61fd37b46feb69b28bb751870dfb02db6f0fa81f68c53`.
Independent checks passed source-archive identity, linked feature providers,
Image/vmlinux architecture, module vermagic and FDT headers/archive identity.
Current-source core regression run `20261015T124900Z-gemjv5a5` also compiled
and linked successfully: 37,519,384-byte Image, 66 AArch64 modules and four
FDTs. Its Image SHA256 is
`36b297a8bd037ef0e9ca2dbec30706ce5673d4d1a47fa8e4c1d86754774da5b1`.
Both builds use kernel release `4.19.325-cip136-st20-perf-g18bd9752ada7-dirty`.

## Ownership and behavior decisions

Mounts keep unique physical IDs from the native IDA. A copied KSU namespace
may present an original legitimate ID while owning a separate reserved
physical ID. A refcounted owner retains the original IDA slot until every
copied view releases it. This prevents reuse after the original mount dies,
without pinning that source mount or pretending a borrowed ID is owned.
Reclassification allocates before releasing the previous slot; native
allocation/vendor-data/group failures follow the native unwind paths.
Hidden status is a private boolean, independent of a high physical ID.

The namespace-copy bit is private and disjoint from the native clone flags.
Native propagation lists, peer-group ownership and Motorola mount-data
callbacks remain in their original functions. Normal/reserved ID ranges
can exhaust and fail allocation; copied views intentionally keep their IDs
reserved until destruction. KSU-origin mounts are classified even after
the sdcard monitor completes, preserving hiding for later root mounts.

Parent traversal is bounded and takes the mount hash lock. Visible-root
helpers return an owned mount+dentry pair, released with `path_put`.
Statfs preserves the original target's LSM decision and the visible target's
native errors. A detached/all-hidden root without a visible ancestor falls
back to native statfs and its own presentation ID; that case is not claimed
to conceal a nonexistent ancestor. Filtered name-to-handle requests for
hidden filesystems return `ENOENT`, rather than an incoherent handle/ID pair.

`STATX_MNT_ID` is backported coherently: the UAPI remains 256 bytes and uses
the original spare slot at byte 144; the result bit, getter, request mask,
copyout and consumers agree. The internal kstat structure grows when mount
support is enabled, so external prebuilt module compatibility remains a
later validation requirement. Public vfsmount layout, Android KABI fields
and Motorola's `.data` member remain unchanged.

Kstat and redirect rules copy immutable data under RCU and gate lookup on a
live inode mapping marker. They retain neither inode nor mount references.
Kstat update resolves the replacement native inode and preserves the
original add-time filesystem/mount snapshot. It honors the tool's update
flags for normal versus full-clone updates; the reference silently ignores
those flags. Selected times, sizes, blocks and 32-bit block size are
validated before publication. Native and ARM32 requests have explicit
layout conversion and checked error-reply address arithmetic.

Redirect resolution occurs after native path completion and before open or
truncation effects. The wrapper restores the original nameidata before one
retry and frees its owned replacement name once. Filename allocation occurs
outside RCU. FUSE redirect registration is explicitly unsupported, and
ambiguous reverse aliases, chains and cycles are rejected. Readlink returns
the copied byte count and allows ordinary truncation. Native custom-link,
permission and copy errors remain observable.

Direct overlay iteration stores one pending entry per open file. The
backing actor only copies the record; native overlay-view lookup follows
after backing iteration releases its lock. A full user buffer, copy error,
seek failure or caller-policy change cannot silently discard that record.
Merged caches retain filesystem identity and never store caller visibility.
Lookup failures propagate. A filtered caller with read permission but no
directory search permission receives `EACCES` when that lookup is required.
This strict behavior and per-entry IO cost need device validation.

Map filtering retains one pagemap slot per page, including mixed visible
and hidden ranges. Remote access rechecks each current VMA/page boundary;
IO fallback cannot cross from a visible VMA into a hidden next VMA. Maps
metadata applies kstat then a matching redirect view, after map hiding.

## Evidence and limits

The current source/host suite has 65 positive configurations: the preceding
45 plus identity/capability 6, map 4, kstat/mount 4, redirect 2 and mount 4.
The seven historical f443 adapter configurations are checked separately.
All 12 command payload sizes and every field offset match pinned JNI on
ARM64 and ARM32; kstat's explicit compat structure matches ARM EABI too.
The statx size/offset probes pass on both targets.

The harnesses execute extracted production/native function bodies with
explicit IDA, VFS, RCU, locks, workqueue, IO and permission models. They do
not execute a booted kernel or establish native race freedom. Targeted ARM64
objects passed for each implemented subsystem before the combined build.
The mount-ID reuse problem was found during review and fixed before enabling
the aggregate; the original pre-fix object diagnostic is retained as history.
Earlier identity diagnostics failed on missing generated native headers;
adding their normal init/SELinux preparation fixed those diagnostic commands.
Fixture failures and expectation updates are not presented as kernel results.

Full target builds must retain CFI/LTO, validate Image and vmlinux architecture,
linked providers, module outputs and devon trees. Subsequent separately
authorized device work must assess boot, SELinux/credential transitions,
namespace cloning/propagation/unmount, mount-ID reuse under load, overlay/FUSE
locks and concurrency, directory offsets/faults, ptrace/map permissions,
multiuser UID schemes and practical compatibility with the target ROM.

Preservation and final current-report/build checks are recorded under
`.backport-context/audit/all-features/`. There is no boot/runtime safety claim.
The final integrity audit passes 125 immutable inputs, all 88 saved preceding
inputs, current report hashes, both full builds, eight individual preparations,
profile refusal checks, 172 document links and preservation of the protected
reference, provider, pristine source repositories and separate non-SUSFS tree.

The incremental checkpatch run is not clean: two reported errors and 12
warnings are retained in `checkpatch.log`. These include the conditional
name-to-handle expression, an assignment-style compat-copy macro, guarded
indentation, extern declarations, aligned-struct parsing and ARRAY_SIZE style.
Compilation and host checks pass; this does not claim style review is complete.
Source whitespace checks pass with the original imported handoff/raw patches
excluded and preserved byte-for-byte.
