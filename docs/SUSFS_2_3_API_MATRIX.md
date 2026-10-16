# SUSFS 2.3.0 interface matrix for Motorola 4.19.325

**Checkpoint extension:** The [reboot/input checkpoint](SUSFS_2_3_REBOOT_INPUT.md) now supplies the SUSFS reboot route and accepted input press hook. The unchanged inline name checker passes with manual-guard warnings; earlier missing-input/manual-only findings below describe the prior checkpoint. Core/payload ABI and ARM64 build remain incomplete.


**Historical checkpoint:** Source selection and SELinux ownership have been superseded by the [automatic-hook checkpoint](SUSFS_2_3_SELINUX_AUTOHOOKS.md). The subsequent [task-state checkpoint](SUSFS_2_3_TASK_STATE.md) records the implemented UID/fork/exec subset. The [access/stat checkpoint](SUSFS_2_3_ACCESS_STAT.md) implements checked kernel-filename consumers. The latest [read/init.rc checkpoint](SUSFS_2_3_INIT_RC.md) adds held-file synchronous RC reads and FD-size reporting. Original source pins/results below are preserved as historical evidence.

Date: 2026-10-13 (Asia/Calcutta). Status: source audit; implementation and target ABI tests pending.

Implementation update: the [SELinux adapter checkpoint](SUSFS_2_3_SELINUX_ADAPTER.md) adds typed consumers/providers and host contract checks. The reference comparison below continues to describe the preserved input sources, not a completed or boot-validated port.

This matrix compares the reconstructed 1.5.9 device port with the pinned SUSFS 2.3.0 and BakaSU snapshots. Input revisions and confidence limits are in [provenance](SUSFS_2_3_PROVENANCE.md). The 1.5.9 patch is not applied to the live kernel checkout.

## Reference notation

Paths beginning M, S and B refer to .backport-context/sources/motorola/, susfs_2_3/ and bakasu/, respectively. Numbers are one-based lines in those preserved snapshots. "Base" refers to the unpatched kernel at 18bd9752ada79090ce11326662eaaa9f33717728. The full pinned BakaSU Kbuild was inspected upstream because it is absent from the snapshot.

Table shorthand: M/susfs.h and S/susfs.h mean the respective include/linux/susfs.h; S/susfs_def.h means S/include/linux/susfs_def.h; B/dispatch.c means B/supercall/dispatch.c; B/sucompat.c and B/sucompat.h mean B/feature/sucompat.c and .h. S/50_add...patch means S/50_add_susfs_in_gki-android12-5.10.patch. These aliases refer only to the preserved inputs, not to generated source.

## Command transport and trust boundary

B/supercall/supercall.c:116 defines:

    int ksu_handle_sys_reboot(int magic1, int magic2,
                             unsigned int cmd, void __user **arg)

It checks KSU_INSTALL_MAGIC1 first (line 118), handles installation separately, requires real UID 0 for extension commands (lines 143-146), and dispatches SUSFS_MAGIC 0xFAFAFAFA (lines 153-156). B/supercall/dispatch.c:1008 implements ksu_handle_susfs_cmd.

The outer argument reference is supplied by the kernel syscall hook as &arg. The payload pointer is *arg. Do not replace this with a userspace pointer-to-pointer decoding convention.

Every SUSFS command handler below has the 2.3.0 signature:

    void handler(void __user **user_info)

Handlers generally read a command-specific structure from *user_info and copy an err field back. Query handlers copy the resulting structure. The dispatcher returns zero even for unknown/disabled commands (B/dispatch.c:1088); a zero syscall/dispatcher result alone is not evidence a feature executed. Preserve and test the actual payload/error convention, including failed copy_to_user and ordinary reboot behavior. The reboot hook is currently absent in SUSFS-only mode.

## Complete pinned command dispatch

| Command / number | Feature guard | BakaSU call | 2.3.0 payload / declaration | 1.5.9 difference |
|---|---|---|---|---|
| ADD_SUS_PATH / 0x55550 | KSU_SUSFS_SUS_PATH | susfs_add_sus_path; B/dispatch.c:1013 | st_susfs_sus_path; S/include/linux/susfs.h:41,191 | int return with typed payload pointer; payload has target_ino and i_uid |
| ADD_SUS_PATH_LOOP / 0x55553 | KSU_SUSFS_SUS_PATH | susfs_add_sus_path_loop; B/dispatch.c:1017 | st_susfs_sus_path; S/susfs.h:192 | No corresponding handler |
| HIDE_SUS_MNTS_FOR_NON_SU_PROCS / 0x55561 | KSU_SUSFS_SUS_MOUNT | susfs_set_hide_sus_mnts_for_non_su_procs; B/dispatch.c:1023 | enabled + err; S/susfs.h:55,197 | Old constant says FOR_ALL_PROCS; old header has no matching new handler |
| ADD_SUS_KSTAT / 0x55570 | KSU_SUSFS_SUS_KSTAT | susfs_add_sus_kstat; B/dispatch.c:1029 | st_susfs_sus_kstat; S/susfs.h:76,202 | int return/typed pointer; changed structure and field semantics |
| UPDATE_SUS_KSTAT / 0x55571 | KSU_SUSFS_SUS_KSTAT | susfs_update_sus_kstat; B/dispatch.c:1033 | st_susfs_sus_kstat; S/susfs.h:203 | Same ABI migration |
| ADD_SUS_KSTAT_STATICALLY / 0x55572 | KSU_SUSFS_SUS_KSTAT | susfs_add_sus_kstat; B/dispatch.c:1037 | Same structure/function; static selection belongs to payload | Verify is_statically semantics; do not invent a separate handler |
| SET_UNAME / 0x55590 | KSU_SUSFS_SPOOF_UNAME | susfs_set_uname; B/dispatch.c:1043 | st_susfs_uname; S/susfs.h:109,208 | Old structure lacks err; old handler returns int |
| ENABLE_LOG / 0x555a0 | KSU_SUSFS_ENABLE_LOG | susfs_enable_log; B/dispatch.c:1049 | enabled + err; S/susfs.h:118,214 | Old susfs_set_log accepts a kernel bool directly |
| SET_CMDLINE_OR_BOOTCONFIG / 0x555b0 | KSU_SUSFS_SPOOF_CMDLINE_OR_BOOTCONFIG | susfs_set_cmdline_or_bootconfig; B/dispatch.c:1055 | 8192-byte array + err; S/susfs.h:126,219 | Raw userspace buffer; old capacity 4096 |
| ADD_OPEN_REDIRECT / 0x555c0 | KSU_SUSFS_OPEN_REDIRECT | susfs_add_open_redirect; B/dispatch.c:1061 | st_susfs_open_redirect; S/susfs.h:134,224 | target_ino removed from payload; uid_scheme and err added |
| ADD_SUS_MAP / 0x60020 | KSU_SUSFS_SUS_MAP | susfs_add_sus_map; B/dispatch.c:1067 | pathname + err; S/susfs.h:156,229 | No corresponding handler |
| ENABLE_AVC_LOG_SPOOFING / 0x60010 | KSU_SUSFS | susfs_set_avc_log_spoofing; B/dispatch.c:1072 | enabled + err; S/susfs.h:163,232 | No corresponding handler |
| SHOW_ENABLED_FEATURES / 0x555e2 | KSU_SUSFS | susfs_get_enabled_features; B/dispatch.c:1076 | 8192-byte output array + err; S/susfs.h:169,234 | int function with char __user * and bufsize |
| SHOW_VARIANT / 0x555e3 | KSU_SUSFS | susfs_show_variant; B/dispatch.c:1080 | 16-byte output array + err; S/susfs.h:175,235 | No corresponding public handler in old header |
| SHOW_VERSION / 0x555e1 | KSU_SUSFS | susfs_show_version; B/dispatch.c:1084 | 16-byte output array + err; S/susfs.h:181,236 | No corresponding public handler in old header |

S/susfs_def.h:12-34 defines the command numbers. Legacy Android-data/sdcard setup, ADD_SUS_MOUNT, ADD_TRY_UMOUNT and SUS_SU commands are deprecated or absent from this dispatch. They must not be carried forward as if current BakaSU invokes the old interfaces.

## Structure and userspace ABI requirements

| Structure | Required audit |
|---|---|
| st_susfs_sus_path | Old layout: unsigned long target_ino, pathname[256], unsigned int i_uid (M/susfs.h:29). New layout: pathname[256], int err (S/susfs.h:41). These are not binary interchangeable. |
| st_susfs_sus_kstat | Old is_statically is int; new is bool. Timestamp seconds/nanoseconds are reordered into pairs; nsec types, blocks/blksize signedness and ordering change; flags and err are appended. Validate every offset, padding and device-number encoding. |
| st_susfs_uname | Two arrays of __NEW_UTS_LEN+1 plus new err; validate termination and output bounds. |
| st_susfs_open_redirect | New pathname[256], redirected_pathname[256], int uid_scheme, int err. Old target_ino-leading payload is incompatible. |
| st_susfs_log / mount control / AVC control | bool enabled followed by int err; padding must be shared with the userspace tool. |
| cmdline / enabled features | 8192-byte arrays plus int err; allocate appropriately, validate lengths/termination, and handle output-copy failures. |
| version / variant | 16-byte arrays plus int err; keep version and capability claims truthful. |

The kernel is ARM64 with CONFIG_COMPAT in the vendor defconfig. A 32-bit process does not give these native long/unsigned long fields the ARM64 layout. Select and document either a tested compat translation or an explicitly supported native userspace tool/ABI; reject unsupported layouts coherently. No sizes or offsets have yet been target-compiled.

Use a single authoritative ABI declaration and target-side offsetof/sizeof checks against the selected userspace tool. Do not make prototype-only changes or rename functions to conceal a payload mismatch.

## Inline hook matrix

| Hook | Device port state | Pinned BakaSU contract | Required adaptation / validation |
|---|---|---|---|
| ksu_handle_execveat | Legacy patch enables existing wrapper with MANUAL or SUSFS guard; base fs/exec.c:1916 | B/feature/sucompat.c:483: int(int *, struct filename **, void *, void *, int *) | Validate su/no-su/static-key conditions and normal/compat exec paths |
| ksu_handle_post_execveat | Called after __do_execve_file; base fs/exec.c:1920 | B/sucompat.c:494 dereferences *filename_ptr; then calls post-exec handler | Base __do_execve_file puts filename at fs/exec.c:1893. Place new hook before release or provide deliberate reference ownership; test exec success and failures |
| ksu_handle_faccessat | Legacy patch enables user-pointer syscall hook; base fs/open.c:454-466 | SUSFS mode requires struct filename ** (B/sucompat.h:17-19; c:527), unlike manual mode | Get/validate kernel filename in the appropriate lookup path; preserve credentials, permission checks and retries. OR-ing guards is insufficient |
| ksu_handle_stat | M/fs/stat.c:390-414 remains MANUAL-only, using const char __user ** | SUSFS mode requires struct filename ** (B/sucompat.c:601) | Move into common stat lookup with correct filename lifetime; cover stat/lstat/fstatat/statx and compat paths |
| ksu_handle_vfs_fstat | No corresponding legacy SUSFS call | B/runtime/ksud_integration.c:683: void(int, loff_t *) | Adjust init.rc kstat size before copying it to userspace; cover read and read_iter consumers |
| ksu_handle_sys_read | M/fs/read_write.c adds a three-argument call before ksys_read, gated by task name init | B/runtime/ksud_integration.c:770: int(unsigned int, char __user **, size_t *) | Existing declaration matches BakaSU. S/50_add...patch:1626 instead declares void(unsigned int); do not import that declaration. Validate intended init credential/static-key checks and first-read behavior |
| ksu_handle_sys_reboot | Base kernel/reboot.c:308-322 remains MANUAL-only; legacy patch does not touch this file | B/supercall/supercall.c:116: int(int, int, unsigned int, void __user **) | Enable correct SUSFS command entry and preserve magic, root checks, output errors and genuine reboot behavior |
| ksu_handle_setresuid | Added before original syscall processing; M/kernel/sys.c | B/hook/setuid_hook.c:181 uses ruid as new_uid and assumes Zygote's equal UID arguments | Audit failed calls and -1/partial UID changes, normal/isolated/WebView Zygote behavior, seccomp and namespace effects |
| ksu_handle_input_handle_event | M/drivers/input/input.c:394 calls after disposition; patch also duplicates entropy call | B/runtime/ksud_integration.c:789: int(unsigned int *, unsigned int *, int *); handler observes volume-down events | Remove duplicate entropy contribution in the new integration. Validate atomic context and key deactivation. Pinned handler does not modify arguments, so ordering is not a demonstrated mutation bug |

The pinned 2.3.0 reference input hook is also after disposition (S/50_add...patch:19-23). Checker warnings do not establish a runtime failure, and absence of a warning does not establish correctness.

The hook checker searches names and legacy guards. Review actual preprocessing, prototypes, objects and link references. Normal direct calls across translation units can link even when independently declared types disagree.

## Core, VFS and runtime dependencies

| Symbol / state | Provider / caller | 4.19 adaptation |
|---|---|---|
| setup_selinux | B/selinux/selinux.c:62; S/fs/susfs.c:29,1480 | Provider exists. Verify kthread credential setup and lifecycle |
| ksu_cred | B/core/init.c:88; S/fs/susfs.c:30,142,1522 | Provider exists. Verify readiness and lifetime before work/credential overrides |
| susfs_is_current_ksu_domain | B/selinux/selinux.c:297; S/fs/susfs.c:28 and namespace patch | Verify domain semantics, early boot and key transitions |
| susfs_priv_app_sid | B/selinux/selinux.c:124; S/50_add...patch:2512 | Provider exists; validate selected SELinux hook configuration and SID initialization |
| susfs_init | S/fs/susfs.c:1530; B/core/init.c:121 | Initialize work before any setuid/unmount hook can schedule it |
| susfs_extra_works | S/fs/susfs.c:1520; B/hook/setuid_hook.c:84 and feature/kernel_umount.c | Absent from 1.5.9 core. Preserve workqueue and no-sleep context contracts |
| susfs_start_sdcard_monitor_fn | S/fs/susfs.c:1509; B/dispatch.c:175 | Triggered after boot-completed event; adapt fsnotify safely and audit missing path/failure/retry behavior |
| susfs_is_sdcard_android_data_not_decrypted | S/fs/susfs.c:1351; namespace/exec/super hooks | Key transition controls early mount classification. Test boot/unlock ordering and missed notifications |
| susfs_get_non_sus_mnt_id_from_mnt | S/50_add...patch:862; S/fs/susfs.c:312,333 | Absent from device port. Bound ancestor traversal and preserve mount-hash locking |
| susfs_get_non_sus_vfsmnt_from_vfsmnt | S/50_add...patch:877; S/fs/susfs.c:313,334 | Returned mount and root must have owned references on every path; pair mntget/dget with mntput/dput, including fallback |
| statfs_by_dentry_wrapper / calculate_f_flags_wrapper | S/50_add...patch:2212,2216; S/fs/susfs.c:277-278 | Underlying static functions exist in base fs/statfs.c:53,47; wrappers are absent |
| susfs_is_inode_sus_path | S/fs/susfs.c:187 under the <6.6 branch | Use inode-only 4.19 signature. The guarded mnt_idmap branch is not evidence this API must be backported |
| kstat/proc spoof helpers | S/fs/susfs.c:546,611,654,675,692 and matching patch callers | Migrate callers together; old inode-number-only helpers are not substitutes |

Full pinned BakaSU Kbuild selects the LSM hooks rather than manager/pkg_observer.o on 4.19 in manual/SUSFS mode. The snapshot's newer fsnotify observer should not be reported as an active 4.19 build error solely from textual presence.

## Kernel API and flag inventory

| Area | Current tree | New contract / caution |
|---|---|---|
| Mount creation | fs_context and vfs_create_mount exist | Adapt to actual Motorola implementation; do not revert to old vfs_kern_mount patch architecture |
| ID allocation | ida_alloc, ida_alloc_min and ida_alloc_range exist | Shared allocator ownership and bounds still require review |
| fsnotify | Base include/linux/fsnotify_backend.h:105 uses handle_event with unsigned-char name and iterator/data arguments | S/fs/susfs.c:1447 uses handle_inode_event with qstr. Adapt callback instead of renaming its member |
| Mark destruction | Base fs/notify/mark.c:206-214 calls ops->free_mark unconditionally | New monitor supplies no free_mark callback. Provide correct callback and balance initial/connector references on success, cleanup and failure |
| FUSE | get_fuse_inode and required inode/nodeid members exist | Validate special handling against this vendor FUSE implementation; header presence alone is insufficient |
| mmap locks | mmap_read_lock helpers already backported | Reuse actual helpers |
| Remote GUP | Base include/linux/mm.h:1536 and mm/memory.c:4496 take task + mm | 5.10 patch uses mm-first form; preserve 4.19 signature and VMA progression |
| statx | Base UAPI lacks STATX_MNT_ID/stx_mnt_id | Decide coherent extension vs explicit native-4.19 limitation. Internal kstat field alone does not implement UAPI |
| Thread info | ARM64 selects THREAD_INFO_IN_TASK; flags is unsigned long | Confirm final CONFIG_64BIT and THREAD_INFO_IN_TASK; audit fork/exec/UID transitions |
| Existing ARM64 TIF | Defined through bit 26 in inspected header | SUSFS 33,34,35 fit 64-bit word with no obvious local overlap |
| BakaSU TIF | IN_KSU_EXECVE=61; DISABLE_ESCAPE_WITH_ROOT=63 in 64-bit mode | Distinct from SUSFS bits; inspect inheritance and clearing, not just numeric uniqueness |
| Address-space markers | Old inode-state flags and mapping bits 28/29 | New AS bits 33,34,35,36,39; preserve atomic updates and inode/mapping lifecycle |
| Mapping GFP mask | Base mapping_gfp_mask uses separate gfp_mask member | No need to assume flags are shared with older packed GFP storage |
| Mount ownership marker | New bit 0x80000000 in int mnt_flags | Audit signedness, flag copy/clear masks and ID lifetime; fresh IDs must not inherit borrowed ownership |
| nameidata / stat markers | New ND flags and private STATX result bits | Audit masks and strip internal bits before userspace copying |
| Vendor build | CONFIG_COMPAT, FUSE_FS, LTO_CLANG, CFI_CLANG in vendor defconfig | Validate with actual merged config and toolchain |

No collision, ABI or lifecycle claim is certified by this table; the listed target and runtime checks remain planned.

## Mount-ID migration contract

1.5.9 assigns special mount/group IDs from separate allocators and stores backup IDs in vfsmount's fourth KABI reserve. Its custom namespace-copy and Zygote renumbering code must be removed or reconciled as a unit.

2.3.0 uses the existing mount/group IDAs with DEFAULT_KSU_MNT_ID=2000000000 and DEFAULT_KSU_MNT_GROUP_ID=200000. Namespace copies may reuse an old ID without allocating it and mark the clone with VFSMOUNT_MNT_FLAGS_KSU_UNSHARED_MNT so teardown does not free that ID. Its unnamed-device allocation uses DEFAULT_KSU_MNT_MINOR_DEV=(1 << 12); verify the effective allocated range rather than relying on the comment's off-by-one wording.

Required invariants:

- Each allocator-owned ID is released exactly once; borrowed IDs are never released by borrowers.
- Fresh allocations clear inherited borrowed-ID markers; error unwind observes the same ownership rule.
- ID ranges, signed integer bounds and exhaustion are deliberate; observable IDs are consistent within a namespace.
- No old backup-ID renumbering path runs alongside the new allocation scheme.
- Ancestor traversal terminates for self-parent roots and detached mounts under correct locking.
- Any returned mount/root pair owns balanced references, including fallback paths.
- Namespace copies, propagation and proc/stat/statfs views share one classification rule.
- Motorola vfsmount.data and vendor ABI reservations/layout remain valid.

See the [ordered implementation and validation plan](SUSFS_2_3_PORT_PLAN.md).
