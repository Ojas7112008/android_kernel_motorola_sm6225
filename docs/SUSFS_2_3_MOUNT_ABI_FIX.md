# SUS_MOUNT module ABI repair

Date: 2026-10-16 (Asia/Calcutta). Experimental local backport.

## Failure and scope

The seven-feature profile (SUS_PATH=y, SUS_MOUNT=n) reached Android, unlock,
BakaSU root and enforcing SELinux. Two all-feature temporary boots returned
to bootloader before ADB. No failing-kernel crash trace survived; the exact
reboot path remains unconfirmed.

Offline comparison identified a concrete module ABI defect. SUS_MOUNT added
an mnt_id field to shared native struct kstat, changing 7,393 common exported
symbol CRCs relative to the working seven build. The installed mmi_info.ko
module has module_layout CRC 0x7b99c8ef; the failed builds use 0x81f987d1.
Fourteen of 42 checked driver imports mismatch those failed builds; all 42
match the working seven build. Prior boot evidence is preserved under
.backport-context/audit/mount-boot-20261016 and its saved Windows diagnostics.

Kernel base: 18bd9752ada79090ce11326662eaaa9f33717728.
BakaSU pin: e1682a2831142e68fa30bab97d5690cf90b75119, with the existing reviewed
provider patch. SUSFS reference: 82f08351fadb8b19dd6591bd14634d02ad832145.
The protected devon-resukisu branch and separate Lineage checkout are untouched.

## Implementation

Restored include/linux/stat.h exactly to the native kernel header. Exported
vfs_getattr_nosec, vfs_getattr and vfs_statx retain their native signatures and
use private helpers accepting an optional u64 mount-ID pointer. Public callers
pass NULL. The statx syscall supplies a local scalar and receives the ID while
the resolved path still owns its mount/dentry references.

The private path retains the LSM check, filesystem getattr error handling,
query masking, BakaSU filename handling, ESTALE retries and reference cleanup.
The mount ID is selected using the existing mount-view helpers. SUS_KSTAT
metadata and its saved mount ID are copied from the same RCU-protected rule
snapshot through susfs_spoof_kstat's private out-parameter.

Only cp_statx writes stx_mnt_id and advertises STATX_MNT_ID. Native kstat
callers do not advertise a field that their structure cannot carry. The
256-byte userspace statx layout and mount-ID offset 144 remain unchanged.
Filesystem callbacks receive the native STATX_ALL request mask.

A complete first-repair build restored module_layout but still differed in
126 of 12,546 common exports. These remaining differences came from SUSFS
fields appended to struct mount in fs/mount.h. Restored that header exactly
to native too. fs/namespace.c now owns a private struct susfs_mount containing
the native struct mount as its first member, followed by SUSFS presentation
ID/class and ID-owner references. The existing mount cache allocates the
larger private object when MOUNT is enabled; allocation, ID ownership,
namespace-copy unwind and free paths keep their existing lifetime behavior.
The native struct mount size/offsets and exported type graph stay unchanged.

The disabled mnt_init and allocation/clone/unwind bodies match native source.
The host fixture uses the actual production cache-size expression and checks
the native-prefix offset; an undersized-cache mutation must fail under ASan.
The implementation changes six kernel files and updates their six hashes
in build-pins.json. Provider/compiler/compatibility checks and version strings
are unchanged. Module version checking and CFI remain enabled.

## Validation and limits

The dedicated test_statx_mount_abi.py suite extracts the actual private and
public getattr/lookup/copy/syscall bodies. Five host configurations cover
native/off, core, KSTAT, MOUNT and MOUNT+KSTAT behavior. Tests cover visible,
hidden, saved-rule and zero mount IDs; generic/FS getattr; AT_EMPTY_PATH and
lookup flags; security denial, lookup/hook/FS/copy errors; ESTALE retry; native
kstat buffer canaries and zeroed statx spare bytes. Four mutations must fail:
lost mount-ID assignment, lost transport, bypassed LSM and early path release.

ARM64 and ARM32 freestanding probes compare native kstat size, alignment and
all field offsets with MOUNT off/on, and check the statx layout. Reintroducing
the old extra kstat member must fail each target's size assertion.

Existing KSTAT real-handler, mount allocator/namespace and access/stat/BakaSU
host suites pass. Native Kbuild/source/provider/compiler/config refusals pass.
These fixtures use explicit ownership, VFS, LSM and RCU models; they do not
establish native races, hardware behavior, rule functionality or stability.

The final fresh all-feature build, 20261016T050934Z-ko0pylez, passes Image,
66 AArch64 modules, four FDTs, all 24 artifact checksums, 29 selected linked
functions and enforcing CFI/LTO. Compiler/linker diagnostics are empty.
All 12,546 reference exports are present with identical CRC/kind/namespace;
all 42 sampled installed mmi_info imports match. Strict incremental checkpatch
has zero errors, warnings or checks. The first completed repair build remains
intermediate evidence and was not device-tested.

The original Axion image was repacked without changing ramdisk, settings,
AVB flags or injecting a modules archive. Pinned AOSP unpack/unsigned AVB
hash/layout checks pass. Boot image SHA256:
2cebd0b4869c60497453c88e3310b9510fb53201b0d58a56efedc9a777c79490.
Windows image:
D:/Phone expriment/Moto G32/Axion/SUSFS-Mount-Fix/20261016T050934Z/boot-bakasu-susfs-test.img.
Boot evidence:
artifacts/susfs-features-test/boot-tests/20261016T052359Z-ssfra2t_.

Fastboot boot reached Android; user unlock, BakaSU root and enforcing SELinux
are verified. The running configuration exactly matches the compiled all
profile, including SUS_MOUNT=y. The user had installed the saved seven-feature
image between sessions; installed boot_b remains its SHA256
3a23c9568ecc94a2a68334575fbf20f4d0a47b33545e37e692b64a9043b315ef.
This task flashed no partition and changed no slot. Reboot returns to that
installed seven-feature baseline.

The same freestanding read-only probe that saw STATX_MNT_ID unavailable on
the seven baseline now passes five paths in both Shell and root contexts:
pathname statx, AT_EMPTY_PATH statx and own fdinfo agree on mount IDs, and
inode identity agrees. Sixty-three modules are Live, including mmi_info and
wlan; this does not establish hardware functionality. Targeted core SUSFS/
BakaSU symbol count remains zero with 184459 other core entries visible;
raw symbol addresses were not saved. The temporary device probe was removed.

Early logs have no detected panic, Oops, BUG, CFI, module-version, unknown-
symbol, refcount or sanitizer failure. Previously observed memcg LRU and
display duplicate-sysfs warnings remain recorded; pstore is empty. This is
one temporary boot with limited runtime checks. Actual hiding-rule behavior,
app namespaces, propagation races, hardware and long-term stability still
need validation. Neither CRC equality nor startup certifies those behaviors.
The exact prior reboot path was not captured, although repairing the two
ABI defects resolves the observed all-feature startup failure in this test.

check_module_abi.py provides the reproducible read-only gate. Dedicated host
regressions are test_statx_mount_abi.py, test_susfs_mount.py,
test_susfs_kstat.py and test_access_stat.py. The native ARM64 probe source is
probe_statx_mount.c; its statx declarations must be generated from this
kernel's UAPI header as recorded in the audit.

See .backport-context/audit/current-mount-abi-fix.txt for exact evidence.
The Windows mount-fix-diagnostics.zip includes the kernel patch, code bundle,
validation manifests, boot logs and smoke results. Changes are local and
uncommitted; there was no push, release or CI dispatch.


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


## Storage cleanup follow-up: 2026-10-16

Regenerable out/ caches and superseded binaries were removed at user request.
The current debug ELF is preserved and byte-verified as
artifacts/susfs-features-test/debug-symbols/20261016T050934Z-ko0pylez/vmlinux.zst;
current Image, modules, DTBs, config, symbol maps and ABI reports remain.
Current and seven-feature recovery boot images are together in the Windows
SUSFS-Mount-Fix/20261016T050934Z folder. Original backups are unchanged.
See the cleanup entry in PORT_LOG.md and .backport-context/audit/current-cleanup.txt.
Old artifact manifests retain their original hashes; CLEANUP.json markers
identify deliberately removed binaries. No kernel source or ref changed.
