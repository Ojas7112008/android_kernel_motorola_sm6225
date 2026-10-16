# SUSFS 2.3.0 core foundation checkpoint

**Checkpoint extension:** The [path core](SUSFS_2_3_PATH_CORE.md) adds an isolated path object and declaration-only header extension. The earlier byte-identical header and next-boundary statements below describe the original foundation checkpoint. Production SUS_PATH remains blocked until its consumers are ported.

Date: 2026-10-14 (Asia/Calcutta). Source/host checkpoint and limited payload-layout probes; no kernel object, full ARM64 build or device operation.

The user authorized starting SUSFS core after the [reboot/input checkpoint](SUSFS_2_3_REBOOT_INPUT.md). Work remains on the isolated susfs-2.3-backport branch. Nine previous inputs/results and the absence of the new core/header are preserved under .backport-context/audit/core-foundation/prior-checkpoint. Exact source pins and the unchanged 16-file BakaSU provider remain in [source-pins.json](../scripts/susfs/source-pins.json).

## Implemented scope

[fs/susfs.c](../fs/susfs.c) implements core query/log handlers, a 4.19 SD-card monitor and statically initialized extra work. [fs/Makefile](../fs/Makefile) selects the core through CONFIG_KSU_SUSFS. The existing pinned BakaSU startup calls susfs_init; its boot-completed dispatcher calls susfs_start_sdcard_monitor_fn. No additional provider patch is required.

[include/linux/susfs.h](../include/linux/susfs.h) is byte-identical to SUSFS 82f0835. Version v2.3.0, NON-GKI selection, constants, payload layouts and public prototypes are preserved. The header contains definitions for later feature work; those definitions do not establish implementation. Version-query output describes the selected interface revision and cannot certify a complete port or boot safety.

The staged configuration requires the checked built-in ARM64 BakaSU route and CONFIG_FSNOTIFY=y. Optional CONFIG_KSU_SUSFS_ENABLE_LOG is implemented. SUS_PATH, SUS_MOUNT, SUS_KSTAT, SPOOF_UNAME, HIDE_KSU_SUSFS_SYMBOLS, SPOOF_CMDLINE_OR_BOOTCONFIG, OPEN_REDIRECT and SUS_MAP cause an explicit compilation error until their core and kernel consumers are ported. Existing BakaSU Kconfig defaults enable several of these; the future development build must deliberately select the staged configuration. Defaults/workflows are not altered to imply a complete build.

The enabled-feature query lists only ENABLE_LOG when compiled; otherwise its result is empty. It zeroes all output and does not echo input names. Version/variant/features are output-only queries with the pinned payload layout. Features allocation failure reports ENOMEM in the payload; inaccessible output can leave the userspace error sentinel unchanged because the pinned dispatcher returns zero. Log requests retain input-copy and err-field reply semantics, reject noncanonical bool bytes with EINVAL, and avoid an invalid C bool load. A reply-copy fault can occur after the log state changes, as in the existing transport convention.

AVC log spoofing has no consumer in this checkpoint. Its existing command returns EOPNOTSUPP through the real payload error field and does not toggle or advertise a capability. Unknown/compiled-out commands retain the pinned dispatcher's zero return and untouched sentinel convention. These are explicit staged limitations; no feature stubs, check suppression or version rewriting are used to claim a green integrated kernel.

## Monitor ownership and execution

The monitor uses the vendor's eight-argument fsnotify handle_event interface and supplies its required free_mark callback. It observes successful directory-open/create/move notifications on /data/media/0; delete/move-self and freeing_mark request watch replacement. The callback only updates atomic state and requests work. It never looks up a path, takes the monitor mutex, destroys a group or disables a static key inside fsnotify's SRCU section.

A statically initialized delayed worker serializes readiness, lookup, watch setup and cleanup. Repeated starts select one monitor. It waits for BakaSU's acquire-published credential readiness and overrides/reverts the immutable KSU credential for each lookup pass. It does not call the old void setup_selinux helper or commit unchecked credentials to a thread.

The worker probes /data/media/0/Android before installing a watch, and every five seconds while pending. A resolved directory closes the monitor and disables the not-decrypted key once. Event names only wake a probe; they cannot open the gate by themselves. Readiness, lookup, permissions, missing directory, allocation and mark-attachment errors keep the key enabled and remain retryable. Initial probing/polling covers pre-existing or missed notifications, and an event during mark registration is tested. This establishes a directory-resolution condition for user zero; it does not prove FBE readiness for all files/users or certify the vendor's actual unlock behavior.

A successful watch owns one path, one group reference and the creator's mark reference. The inode pointer is borrowed while the path is held; fsnotify owns its separate connector reference. Setup failures release all acquired resources, preserve the actual error and publish no partial watch. Cleanup drops the creator mark before group destruction removes the attachment and waits for deferred frees, then drops/zeros the path. This avoids retaining the mark/group after fsnotify detaches the mark. Worker cleanup can hold the monitor mutex because callbacks never acquire it.

Extra work is initialized at compile time, avoiding a schedule against an uninitialized work object. It requires credential readiness. With SUS_PATH intentionally disabled it follows the upstream empty conditional-work behavior; the actual path-loop body remains for the next feature series. The built-in monitor has no module-unload contract.

## Executed validation

[test_susfs_core.py](../scripts/susfs/test_susfs_core.py) compiles the whole staged core, actual pinned reboot provider/command dispatcher, and extracted vendor mark initialization/final-free/group-ref/group-destruction functions. [test_susfs_core.c](../scripts/susfs/test_susfs_core.c) models VFS, user copies, workqueue scheduling, connector references, SRCU and host locks. It does not execute the real fsnotify traversal/reaper, workqueue or SELinux/VFS subsystem.

Four Clang 21.1.8 ASan/UBSan configurations pass: log off/on with the BakaSU static-key compatibility macro absent/defined. Core keys remain real static keys in both modes. Checks cover credential/readiness restore, allocation/watch errors and retries, null/non-directory inodes, missed/pre-existing events, event during registration, watch detachment/replacement, exact creator/attachment/group/path balance, cleanup outside SRCU, concurrent starts/events/work passes and one-way completion. Command tests execute root-gated transport through actual dispatch into the handlers, including zeroed output tails, allocation/read/write faults, invalid log booleans, unavailable AVC and unchanged unknown-command sentinels.

Negative controls reproduce the pinned feature query echoing caller-supplied names for a compiled-out feature, and its actual watch constructor/cleanup retaining the creator mark/group under the vendor's reference model. The latter model supplies a valid free callback to isolate the ownership leak; the original modern callback/ops separately fails compilation against the extracted 4.19 function table. Tests recover deliberately leaked control references so the harness has no leak. Every unported feature flag and absent FSNOTIFY also fails compilation explicitly.

Freestanding Clang probes for aarch64-linux-android and armv7a-linux-androideabi validate sizeof and err offsets for five fixed-layout core payloads against the exact JNI declarations: version/variant 20 bytes with err at 16; features 8196 with err at 8192; log/AVC 8 with err at 4. BakaSU's corresponding Rust repr(C), bool/i32 fields are inspected. These syntax probes are not Android kernel object builds, Rust compilation, a complete feature ABI audit or real ARM32 transport validation. Native-long feature payloads remain unvalidated.

Evidence is under .backport-context/audit/core-foundation. Prior source/host regressions, immutable input/ref preservation, result hashes and style outcomes are recorded in [PORT_LOG.md](PORT_LOG.md). All 29 current positive host configurations and both target payload probes pass. The final audit verifies 125 immutable inputs, protected/original state, nine preserved inputs, recorded result hashes and 92 local document links. The core/Makefile patch has zero checkpatch errors and one new-file MAINTAINERS warning; the imported byte-identical header retains three spacing errors and five warnings. There is no global clean-style claim. Evidence is .backport-context/audit/core-foundation/final-integrity.json.

## Next boundary

Port the feature core bodies and their consumers in dependency order, starting with reviewed path-list/SRCU and mount-ID ownership contracts. Complete the remaining payload/native/compat ABI and filesystem/procfs/AVC interfaces before enabling their flags. Then update the legacy workflow to exact source/config/toolchain pins and run real ARM64 object/full-kernel gates. Lockdep, real fsnotify grace periods, FBE, seccomp/FD scope, CFI/LTO, full boot and device runtime remain unvalidated.
