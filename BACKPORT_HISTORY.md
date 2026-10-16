# Current checkpoint — 2026-10-16

The dated handoff below records the original project state, before the subsequent
backport work in [docs/PORT_LOG.md](docs/PORT_LOG.md). Its pending-work/status
statements are historical. The current build instructions and branch mapping
are in [README.md](README.md) and [docs/DEVON_BUILDS.md](docs/DEVON_BUILDS.md).

The new fork uses clean LineageOS 23.2 base c65462bf08da2a8900b98e705bea376de7f7f19c.
Default `lineage-23.2` contains the BakaSU/SUSFS 2.3 integration;
`lineage-23.2-bakasu` contains the separately pinned non-SUSFS integration.
All eight selected SUSFS features are present, with AVC log spoofing unsupported.
Earlier builds passed limited boot/root/module/mount-ID checks; runtime hiding
rules and long-term stability remain unvalidated. All 55 production backport
files are unchanged in this migration. Native provider compatibility/version
checks and CFI/LTO remain active.

Local configuration, native Kbuild checks, preparation negative controls and
17 maintained host/ABI suites passed. A separate fresh-checkout preparation also
passed without old fork refs or dependency caches. Two first GitHub builds are
running; their results are not yet a successful-build claim. No phone operation
was performed during publication. The redundant local non-SUSFS directory was
removed after remote-ref verification and independent source/submodule backups.
The protected legacy reference remains preserved locally.

---

# Moto G32 (`devon`) — BakaSU / ReSukiSU + SUSFS 2.3.0 Backport Handoff

**Prepared:** 2026-10-13  
**Purpose:** Transfer technical context, experiments, fixes, failures, evidence, and next steps from a long ChatGPT troubleshooting session to a **new Codex coding session**.  
**Status:** The **non-SUSFS** BakaSU kernel has booted and supported root in testing. The **SUSFS 1.5.9** port has progressed through patch and hook-check stages but has **not** produced a verified bootable SUSFS kernel. SUSFS **2.3.0 → Motorola Linux 4.19.325** is the active development goal.

> **Read this before editing anything.** This is an engineering handoff, **not** a certification that the code is correct. Many details come from prior troubleshooting reports; the most recent source archive and diagnostic log were inspected where indicated. Verify the current repository, branch and staged/unstaged edits before assuming any file exactly matches the snapshots here. Preserve the working branch and original Axion boot image. Do not silently bypass compatibility checks.

---

## 1. TL;DR for the next Codex agent

- **Device:** Motorola Moto G32, codename `devon`; Qualcomm SM6225 family; A/B partitions, no standalone recovery partition. Running unofficial **AxionAOSP 2.8**, **Android 16 QPR2**.
- **Kernel:** Motorola/LineageOS-derived **Linux 4.19.325**, **non-GKI**. Source lineage: [`LineageOS/android_kernel_motorola_sm6225`](https://github.com/LineageOS/android_kernel_motorola_sm6225) and device tree [`LineageOS/android_device_motorola_devon` branch `lineage-23.2`](https://github.com/LineageOS/android_device_motorola_devon/tree/lineage-23.2).
- **User's repo:** https://github.com/Ojas7112008/android_kernel_motorola_sm6225
- **Working (non-SUSFS) branch:** `devon-resukisu` — **protect this**.
- **Experimental SUSFS branch:** `devon-resukisu-susfs` — development starting point; use a new, separate worktree/branch for the 2.3.0 port.
- **BakaSU:** v4.2.0-rc3, version code 35227; failed SUSFS action showed `f4435f00`. Exact full commit must be taken from `artifacts/resukisu-commit.txt` / GitHub Actions and pinned. Current 2.3.0 source ZIP embeds a BakaSU snapshot but does **not** include BakaSU `.git` metadata.
- **Old SUSFS reference:** `kernel-4.19` branch, v1.5.9, previously pinned at `1f764cfb8ddc3b19d7624731051d1000d629175b` from `ShirkNeko/susfs4ksu` / simonpunk GitLab mirror.
- **New SUSFS reference:** [`ShirkNeko/susfs4ksu`](https://github.com/ShirkNeko/susfs4ksu), `gki-android12-5.10` branch, v2.3.0, **commit `82f08351fadb8b19dd6591bd14634d02ad832145`** in diagnostics. Upstream GitLab source: https://gitlab.com/simonpunk/susfs4ksu . This is a *reference implementation*, **not a directly applicable 4.19 patch**.
- **Existing port patch:** `.github/patches/devon-susfs-4.19.patch` (~117 KB in provided ZIP; 23 modified/added paths; contains `drivers/input/input.c` hook). This targets the **1.5.9 port**, **not** the final 2.3.0 port.
- **Main blocker:** `KernelSU/kernel/tools/susfs_compat.mk` in the inspected BakaSU snapshot has an **unconditional `$(error Please wait susfs update.)`** at line 20. Upgrading SUSFS alone will **not** make that revision build. The correct solution requires validating/updating both sides of the interface and explicitly handling this version-pinned restriction; **do not simply delete it as a shortcut**.
- **Important correctness warning:** BakaSU's `inline_hook_check.mk` primarily `grep`s for hook names and for `CONFIG_KSU_MANUAL_HOOK`. “Found” messages do **not** prove a hook is compiled, linked, correctly placed, or reachable under `CONFIG_KSU_SUSFS=y`.

### Definition of success

A reviewed, version-pinned SUSFS 2.3.0-equivalent integration that builds with the selected BakaSU revision on this **exact** Motorola 4.19 tree, with verified ABI/function compatibility, correct VFS/mount namespace semantics, a reproducible GitHub Actions build, and careful **temporary** device boot testing before considering permanent installation. **Compilation alone is not success.**

---

## 2. Evidence and confidence

| Evidence | What it establishes | Limits |
|---|---|---|
| Prior user-reported console / GitHub Actions results | Successful non-SUSFS boot, `uid=0`, earlier failed build stages, experiments and workflow debugging | May not exactly match current checkout; original full logs are not all available here |
| `susfs-diagnostics.log` | v2.3.0 reference SHA; diffs of `susfs_def.h`; searches for new dependencies; FUSE header present | Dependency grep did **not** search BakaSU sources; “not found” is not proof of absence |
| `susfs-backport-sources.zip` | 120 source/patch files in `motorola/`, `susfs_2_3/`, and `bakasu/` snapshots; exact signatures and hooks | Selected files only, not a complete checkout; ZIP does not include `.git` metadata |
| `susfs_4_19_backport_audit.md` | Earlier source-derived incompatibility audit and proposed migration sequence | Audit only; no full integration or boot test |
| Actual repository and Actions artifacts (to be re-inspected by Codex) | Authoritative current checkout, commit, workflow, build configuration | Not automatically transferred to a new local clone unless committed/pushed |

**Not verified:** a successful SUSFS-enabled compile/link, SUSFS 2.3.0 functionality on this 4.19 kernel, mount-ID/refcount safety, Android boot stability with SUSFS, proper all-hook execution in SUSFS mode, or compatibility with a newer BakaSU release.

---

## 3. Known device, build and boot-image context

- Target is ARM64, Motorola devon with **A/B slots**. Root methods previously used include APatch and ReSukiSU/BakaSU. The working non-SUSFS build supports KernelSU-style root; a later test command `adb shell su -c id` reported `uid=0` and SELinux context `u:r:ksu:s0`. An earlier session had `su: not found`, later resolved during the build/integration sequence. Do not conflate the two tests.
- The working kernel was tested with **`fastboot boot`**, not simply flashed permanently. Wi-Fi/internet appeared functional, and `adb shell lsmod` showed loaded modules (including Wi-Fi/audio components). This validates only the **non-SUSFS** build as tested.
- Kernel sources contain vendor config fragments:
  - `arch/arm64/configs/vendor/bengal-perf_defconfig`
  - `arch/arm64/configs/vendor/ext_config/moto-bengal.config`
  - `arch/arm64/configs/vendor/ext_config/devon-default.config`
- CI uses Ubuntu 22.04, Android 16 Clang (`android-16.0.0_r4/clang-r547379`), ARM64 cross tools, typically `make O=out ARCH=arm64 LLVM=1` with Image, DTBs and modules.
- Original Axion 2.8 `boot.img` baseline is stored as a **compressed GitHub Release asset**, tag `axion-2.8-boot-base`, asset `axion28-devon-original-boot.img.zst`. In inspected workflow source, decompressed baseline SHA-256 check is:

  ```text
  1632d107116564f6130b8633c5c21b09261009a03712e086dd22cb9e71edc4de
  ```

  Repacking code is tailored to this boot image's Android boot v3 layout/ramdisk/header and AVB details. **Do not generalize it to other ROM versions or boot images.** A provided working artifact name from the earlier experiment was `devon-resukisu-test-boot.img`.
- GitHub Actions workflow path reported in repo: `.github/workflows/build-devon.yml`, using `workflow_dispatch` and `resukisu_ref` (normally `main`) and `build_mode` (`manual` or `susfs`). Versions of workflow YAML (`build-devon.yml`, `build-devon-improved.yml`, `build-devon-susfs-experimental.yml`) were separately inspected in session files. **Read the current branch's exact `.github/workflows/build-devon.yml`**; not all copies are guaranteed to be identical.
- The GitHub Actions UI initially did not show the workflow; adding `workflow_dispatch` to the default branch `devon-resukisu` made it available. The user could then select experimental branch and SUSFS mode in the run dialog.
- `tar: dtbs: Cannot stat: No such file or directory` was fixed by archiving actual device-tree outputs under `out/arch/arm64/boot/dts/` instead of assuming a directory named `dtbs` exists.
- Building/repacking the **known-working manual mode** and generating `artifacts/boot.img` succeeded previously. **Do not overwrite that release with an untested SUSFS image.**

---

## 4. Chronology: attempts, failures and fixes

### Phase A — Establish a working BakaSU non-GKI kernel (2026-10-12)

1. Explored multiple SM6225 kernel repositories/forks, then established the GitHub repository above based on LineageOS kernel sources; use branch/commit history to verify exact ancestry.
2. Wired BakaSU/ReSukiSU into the 4.19 kernel using GitHub Actions and **non-GKI manual hooks**. Previous manual hooks include `ksu_handle_execveat` and `ksu_handle_faccessat`. `ksu_handle_stat` was initially absent from the checked code, prompting exploration of `SYSCALL_DEFINE2(newstat)`, `SYSCALL_DEFINE2(newlstat)` and `SYSCALL_DEFINE4(newfstatat)`; this alone did not settle the final SUSFS integration.
3. Fixed workflow dispatch visibility and DTB packaging. Generated a bootable kernel and repacked boot image from the **exact ROM-specific Axion original**, not a generic boot image.
4. Used `fastboot boot` with a resulting boot image; reported successful boot, Wi-Fi/internet, loadable modules, and subsequently working shell root (`uid=0`). **This is the recovery baseline.**
5. Asked to add **SUSFS**, moving to a separate experimental branch so the known-working image remains intact.

### Phase B — Attempt the legacy SUSFS 4.19 patch (2026-10-12 to 2026-10-13)

1. Obtained SUSFS v1.5.9 from the `kernel-4.19` source branch; GitLab fetching sometimes failed, so used GitHub mirror fallback. A direct `git apply --check` of `50_add_susfs_in_kernel-4.19.patch` failed.
2. **Root cause:** This Motorola 4.19 source already contains a **backported `fs_context`/`vfs_create_mount()` architecture**, while the older SUSFS patch targets older `vfs_kern_mount()` behavior and different `fs/namespace.c` layouts. **Kernel version equality is insufficient** to apply the patch verbatim.
3. In GitHub Codespaces manually integrated rejected hunks after `git apply --reject`. Known rejects/conflicts included `fs/Makefile`, `include/linux/mount.h`, and `fs/namespace.c`; many other hunks applied or were adapted.
4. Added `obj-$(CONFIG_KSU_SUSFS) += susfs.o` to `fs/Makefile`; populated `fs/susfs.c`, `include/linux/susfs.h`, and `include/linux/susfs_def.h` using the 4.19 SUSFS reference.
5. Preserved Motorola's `void *data` in `struct vfsmount`; adapted its fourth Android KABI reserve with a guarded `ANDROID_KABI_USE(4, u64 susfs_mnt_id_backup)`. **Review this layout for vendor ABI compatibility; do not assume a build validates it.**
6. In `fs/namespace.c`, added SUSFS declarations and separate ID allocators (`susfs_mnt_id_ida`, `susfs_mnt_group_ida`), `CL_COPY_MNT_NS`, mount namespace logic, and a modified `alloc_vfsmnt()` path under SUSFS config. In `vfs_create_mount()` adapted the call to something of the form `alloc_vfsmnt(fc->source ?: "none", unlikely(susfs_is_current_ksu_domain()), 0)` for the SUSFS conditional path; introduced Zygote-related original/fake mount-ID backup handling. **These modifications were not runtime validated and are high-risk.**
7. Adjusted `fs/stat.c` to avoid releasing `path` (`path_put(&path)`) before the SUSFS path-dependent logic completed.
8. Generated the repo-tracked custom port patch from source diffs. As added files were untracked, used `git add -N` to include them in `git diff --binary`; validated `git diff --check` and `git apply --check` in a detached **clean worktree**. Because `.github` can be ignored by this kernel's `.gitignore`, staging required `git add -f .github/patches/devon-susfs-4.19.patch`. **Only stage the patch, not incidental `.rej` files or the entire local modified tree.**
9. After the input hook was later added, the patch generator had to include `drivers/input/input.c` explicitly: `git diff --binary -- fs include kernel drivers/input/input.c > .github/patches/devon-susfs-4.19.patch`. The uploaded archive contains this input-hook change.

### Phase C — BakaSU SUSFS inline-hook checker failures and incremental fixes

For SUSFS mode, `CONFIG_KSU_SUSFS=y` and `CONFIG_KSU_MANUAL_HOOK=n` are expected; the hook checker lives in `KernelSU/kernel/tools/inline_hook_check.mk` after BakaSU setup. In the uploaded BakaSU snapshot this checker is a textual grep-based gate.

| Error / warning seen | Action taken | State / remaining concern |
|---|---|---|
| `ksu_handle_setresuid` missing in `kernel/sys.c` | Declared and invoked `ksu_handle_setresuid(ruid, euid, suid)` near `long __sys_setresuid(uid_t ruid, uid_t euid, uid_t suid)`, under `CONFIG_KSU_SUSFS`. | Name detected by checker; semantics must still be reviewed. |
| `ksu_handle_sys_read` missing in `fs/read_write.c` | Declared `int ksu_handle_sys_read(unsigned int fd, char __user **buf_ptr, size_t *count_ptr)`; inserted call from `SYSCALL_DEFINE3(read, ...)`, guarded for task name `init`, before `ksys_read(...)`. | Name detected; compare exact hook placement/signature to pinned BakaSU before trusting. |
| `ksu_handle_input_handle_event` missing in `drivers/input/input.c` | Added declaration and call in `input_handle_event()` after `input_get_disposition(...)`, guarded by `CONFIG_KSU_SUSFS`. | Checker recognizes it; **must audit call ordering**—changing event type/code after computing `disposition` may be incorrect. BakaSU uses different static-key logic in SUSFS mode, so avoid manually referencing old `ksu_input_hook` variable. |
| Warnings for `CONFIG_KSU_MANUAL_HOOK` in `fs/exec.c` and `fs/open.c` | Previously changed portions to `#if defined(CONFIG_KSU_MANUAL_HOOK) || defined(CONFIG_KSU_SUSFS)`. | Grep checker may still warn even with correct OR guard; compile and inspect preprocessed code. |
| Warnings for `CONFIG_KSU_MANUAL_HOOK` in `fs/stat.c` and `kernel/reboot.c` | **Not fully resolved.** | Uploaded `motorola/fs/stat.c` still wraps both `ksu_handle_stat` and its call with `#ifdef CONFIG_KSU_MANUAL_HOOK`; under SUSFS-only mode these are compiled out. `kernel/reboot.c` was not included in the uploaded source snapshot. Inspect the actual tree. |
| Earlier `ksu_handle_stat` implementation / other stat hooks | Some source lines contain `ksu_handle_stat`, `ksu_handle_newfstat_ret`. | A checker finding a string is **not evidence these hooks actually compile/execute**; protect syscall ABI and signedness. |

**Crucial checker implementation facts** from BakaSU snapshot:

```makefile
# It checks simple textual presence:
$(eval $(call check_ksu_hook,ksu_handle_stat,$(srctree)/fs/stat.c))
# It separately greps for CONFIG_KSU_MANUAL_HOOK and emits warnings:
$(eval $(call check_ksu_manual_guard,$(srctree)/fs/stat.c))
```

The checker also declares older symbols incompatible: `ksu_vfs_read_hook`, `ksu_input_hook`, `ksu_execveat_hook`, and `ksu_init_rc_hook`. Verify all relevant hooks in the **final generated/preprocessed SUSFS-mode code**, not only by grep.

### Phase D — All hook names pass, BakaSU compatibility blocker remains

The latest failure after adding the input hook was:

```text
BakaSU: SuSFS has NOT update with latest KernelSU source
KernelSU/kernel/tools/susfs_compat.mk:20:
*** Please wait susfs update. Stop.
```

The uploaded BakaSU source `bakasu/tools/susfs_compat.mk` confirms lines 18–20 contain:

```makefile
$(info -- $(REPO_NAME): susfs has NOT update with latest KernelSU source)
$(info -- $(REPO_NAME): Please wait simonpunk update)
$(error Please wait susfs update.)
```

**There is no surrounding condition on these lines**. This restriction blocks every evaluated build of that compatibility fragment, regardless of SUSFS header version. It represents an explicit upstream stop sign, **not** a generic version comparator or an error resolved by changing `#define SUSFS_VERSION`. Do not suppress it until source compatibility has been checked and a deliberate integration strategy chosen. Pin BakaSU to an exact revision instead of letting `main` drift.

### Phase E — Examine SUSFS 2.3.0 to plan a real backport (2026-10-13)

1. Cloned `gki-android12-5.10` SUSFS reference into Codespaces under `$HOME/susfs-backport/susfs-5.10/`, instead of volatile `/tmp`.
2. Reference SHA in generated diagnostics: `82f08351fadb8b19dd6591bd14634d02ad832145`, `SUSFS_VERSION "v2.3.0"`.
3. Compared `fs/susfs.c`, `include/linux/susfs.h`, and `include/linux/susfs_def.h` against Motorola's v1.5.9 port. Observed **substantial redesign**, not a handful of commits.
4. Generated and uploaded `susfs-diagnostics.log`. Its symbol search found none of `setup_selinux`, `ksu_cred`, `susfs_get_non_sus_mnt_id_from_mnt`, `susfs_get_non_sus_vfsmnt_from_vfsmnt` in *selected Motorola directories*. Later inspection of the BakaSU source correctly located `setup_selinux` in `bakasu/selinux/selinux.c` and `ksu_cred` in `bakasu/core/init.c`. Mount helpers appear in the SUSFS **5.10 reference patch**, not the existing 4.19 port; they still need device-specific implementations.
5. Confirmed `fs/fuse/fuse_i.h` exists in the Motorola tree, **but existence alone does not guarantee the required FUSE internals/API are compatible**.
6. Uploaded `susfs-backport-sources.zip` (120 paths, selected Motorola files + SUSFS 2.3.0 reference + BakaSU code) and produced a source-audit report `susfs_4_19_backport_audit.md`. It is a **diagnostic snapshot**, not an edited 2.3.0 implementation.
7. Next planned read-only test was `git apply --check --verbose` of `susfs_2_3/50_add_susfs_in_gki-android12-5.10.patch` against a **clean detached worktree**; there is **no supplied resulting compatibility log yet**. Expect many conflicts; this is reconnaissance, not a way to directly apply 5.10 code to 4.19.

---

## 5. Source-derived incompatibilities: v1.5.9 versus v2.3.0

### 5.1 Kernel/BakaSU command API is different

In uploaded snapshots:

```c
/* Motorola v1.5.9 header */
int susfs_add_sus_path(struct st_susfs_sus_path __user *user_info);

/* SUSFS v2.3.0 header */
void susfs_add_sus_path(void __user **user_info);
void susfs_add_sus_path_loop(void __user **user_info);
```

BakaSU `bakasu/supercall/dispatch.c` calls v2.x style handlers with `void __user **arg`, including `susfs_add_sus_path_loop`, `susfs_set_hide_sus_mnts_for_non_su_procs`, and `susfs_add_sus_map`. These are **API and userspace ABI** changes—simple renaming, prototypes-only updates or spoofed version numbers will not work. Check **every** command handler, struct layout, copy-from/to-user direction, return/error convention, alignment, and permission gate.

### 5.2 Definitions and feature semantics changed

`include/linux/susfs_def.h` v2.3.0 adds or changes:

- `SUSFS_MAGIC 0xFAFAFAFA`; `CMD_SUSFS_ADD_SUS_PATH_LOOP 0x55553`;
- `CMD_SUSFS_HIDE_SUS_MNTS_FOR_NON_SU_PROCS 0x55561` replacing `...FOR_ALL_PROCS` semantics;
- `CMD_SUSFS_ENABLE_AVC_LOG_SPOOFING 0x60010` and `CMD_SUSFS_ADD_SUS_MAP 0x60020`;
- many legacy commands marked deprecated, including `...ADD_SUS_MOUNT`, `...ADD_TRY_UMOUNT`, and older SUS_SU modes;
- cmdline/bootconfig payload capacity `4096` → `8192` bytes, and new maximum output sizes;
- old mount constants `DEFAULT_SUS_MNT_ID=100000`, `DEFAULT_SUS_MNT_ID_FOR_KSU_PROC_UNSHARE=1000000`, `DEFAULT_SUS_MNT_GROUP_ID=1000` replaced by `DEFAULT_KSU_MNT_ID=2000000000`, `DEFAULT_KSU_MNT_GROUP_ID=200000`, plus device-minor conventions;
- old `INODE_STATE_SUS_*` flags replaced in many paths with `AS_FLAGS_*` on `inode->i_mapping->flags`; new `VFSMOUNT_MNT_FLAGS_KSU_UNSHARED_MNT=0x80000000`;
- newer thread flags `TIF_PROC_UMOUNTED=33`, `TIF_PROC_NO_SU=34`, `TIF_PROC_UMOUNTED_FOR_ZYGOTE_NEXT=35`, plus helpers to query/set/clear them; `STATX_SUS_KSTAT` and `STATX_SUS_KSTAT_FUSE` request bits.

**Check collisions and correct word sizes**, especially `thread_info.flags` on ARM64, address-space flags and signed `vfsmount.mnt_flags`. The uploaded Motorola ARM64 `thread_info.h` lists existing TIF definitions up to 26; the uploaded BakaSU SUSFS-mode definitions use other high bits. **No obvious overlap in those snapshots** is *not* proof of safe availability across all vendor trees/config variants.

### 5.3 `fs/susfs.c` and structures underwent a rewrite

- v1.5.9 uses spinlock-centric code and inode-state flags in several hooks. v2.3.0 introduces `mutex`, `SRCU`, and static keys in some areas.
- SUS path handling changes request structures and error propagation; v2.3.0 uses FUSE internals (`"fuse/fuse_i.h"`, `get_fuse_inode`) for some cases and changes path lookup behavior.
- SUS kstat adds field-selective spoof flags, extra `kstatfs`/mount data and error fields. `st_susfs_sus_path`, mount controls and log/uname commands change layouts.
- BakaSU features refer to `susfs_extra_works` and the SUSFS `proc_no_su` helpers; these are provided in 2.3.0 but **not** in current v1.5.9 port.
- SUSFS 2.3.0 `fs/susfs.c` names `setup_selinux`, `ksu_cred`, `susfs_get_non_sus_mnt_id_from_mnt`, `susfs_get_non_sus_vfsmnt_from_vfsmnt`. BakaSU provides the first two, the latter two are implemented in **reference kernel patch** and need Motorola-specific VFS work.
- New 5.10 integration also refers to `statfs_by_dentry_wrapper` and `calculate_f_flags_wrapper`, absent from existing Motorola port and found in the reference patch. Review `fs/statfs.c`, struct versions, and caller lifetime.

### 5.4 The kernel patches cover different subsystems

The SUSFS 2.3.0 Android 5.10 reference patch modifies **24 kernel paths** and includes `fs/proc/base.c`, `fs/proc/bootconfig.c`, `fs/super.c`, `mm/memory.c`, and SELinux modifications. The uploaded Motorola v1.5.9 custom patch modifies/adds **23 paths** but not necessarily the same paths. The full Motorola checkout, including `mm/` and SELinux, is needed for a real port.

**Especially risky:** do **not** layer the 2.3.0 mount-ID assignment, backing IDs, namespace-copy semantics and procfs spoofing on top of the existing 1.5.9 spoof-ID logic. Coordinate ID allocation/free, reference counting, namespace clone/attach/detach, bind mounts, Zygote/app views, and procfs exposed IDs as one system. Avoid collisions and use of freed mount objects.

### 5.5 Existing manual/inline-hook concerns carry forward

- Uploaded `motorola/fs/stat.c` still contains `#ifdef CONFIG_KSU_MANUAL_HOOK` around `ksu_handle_stat`; in intended SUSFS mode manual-hook config is **off**.
- Uploaded `motorola/drivers/input/input.c` calls `ksu_handle_input_handle_event(&type, &code, &value)` *after* `input_get_disposition(dev, type, code, &value)`. Investigate whether this placement is correct for BakaSU's semantics. Compare pinned upstream integration and event propagation logic before treating it as fixed.
- `bakasu/runtime/ksud_integration.c` uses `DEFINE_STATIC_KEY_TRUE(ksu_is_input_hook_enabled)` for SUSFS mode. Older `ksu_input_hook` flags belong to other modes; the checker explicitly rejects legacy names in source.
- Verify actual calls, prototypes and **link-time** symbols by examining generated preprocessing/`nm`/`vmlinux` or relevant objects, not just a textual grep.

---

## 6. What must **not** be assumed or repeated

1. **Do not** edit the stable `devon-resukisu` branch in place. Create a dedicated `susfs-2.3-backport` branch/worktree based on the latest **verified** experimental baseline, after recording commits and dirty status.
2. **Do not** try to apply a complete Linux 5.10 patch to Motorola 4.19 and accept `--reject` output as a solution. Review all rejected context and interfaces by hand.
3. **Do not** replace `fs/susfs.c` or the headers with a newer copy without updating all caller interfaces/hook sites in sync.
4. **Do not** remove `$(error ...)` or set `SUSFS_VERSION="v2.3.0"` merely to make a build pass. Address BakaSU's deliberate source restriction only after a documented, cross-checked ABI integration decision.
5. **Do not** assume `CONFIG_KSU_MANUAL_HOOK` and `CONFIG_KSU_SUSFS` can both be enabled; current workflow uses **SUSFS mode instead of manual mode**.
6. **Do not** interpret grep hook-check success or `git apply --check` as compilation or runtime proof.
7. **Do not** permanently flash an experimental boot image or overwrite a ROM-specific stock image/release. Preserve recovery capability and use fastboot temporary boot only after preflight review and user consent.
8. **Do not** use `main` as an unpinned floating dependency for reproducible backport test builds. Capture source SHAs and artifacts.
9. **Do not** put the 96 MB boot image, signing materials, credentials or sensitive phone data in commits. Archive/source diagnostics can be shared after secret review.
10. **Do not** assume `/tmp/bakasu-inspect` or `/tmp/devon-susfs-check` survives a Codespaces restart. Reference clones were moved to `$HOME/susfs-backport` for persistence.

---

## 7. Next tasks, in recommended order

### Stage 0 — Repo and evidence inventory (read-only)

- Confirm location (`pwd`), branch, HEAD SHA, dirty status and existing patch path. Check exact GitHub Actions workflow content and latest failure log; preserve an immutable note of the BakaSU commit and both SUSFS commits.
- Distinguish **tracked committed** changes from **uncommitted** Codespaces edits. A fresh PC/WSL clone may lack locally modified `fs/*`, `include/*`, the `$HOME/susfs-backport/susfs-5.10` clone, and downloaded files.
- Inspect `susfs-backport-sources.zip`, `susfs-diagnostics.log`, and `susfs_4_19_backport_audit.md` if available. These are historical snapshots only; compare to live tree.
- Create an isolated development worktree/branch for porting. No behavior-changing edits yet.

Suggested diagnostics (safe to run in repo; **do not** use with `git reset --hard`):

```bash
pwd
git remote -v
git branch --show-current
git rev-parse HEAD
git status --short
ls -l .github/patches/devon-susfs-4.19.patch .github/workflows/build-devon.yml 2>&1
find . -maxdepth 2 -name 'KernelSU' -type d -print
```

### Stage 1 — Freeze inputs and draw the API matrix

- Obtain **exact** BakaSU commit used by failed CI, preferably from its `artifacts/resukisu-commit.txt`. Compare source snapshot to that checkout.
- Pin SUSFS 2.3.0 reference SHA `82f08351fadb8b19dd6591bd14634d02ad832145`; keep v1.5.9 pinned as baseline; record kernel HEAD.
- Enumerate all BakaSU references to SUSFS functions/macros/structs; map definitions in 2.3.0 and call sites in Motorola 4.19 patch. Include `copy_to_user`, `copy_from_user`, `void __user **arg`, feature flags, init/fini paths and errors.
- Inventory new Kconfig dependencies; compare all affected `CONFIG_KSU_SUSFS_*` modes with intended options. Disable features **only as documented and coherent**, not to hide linker failures.
- Check compatibility guard intent and whether an updated BakaSU revision provides a supported route. Document the exact BakaSU + SUSFS pairing decision before any guard modification.

### Stage 2 — Upstream patch applicability report only

- On a **clean worktree** at known Motorola HEAD (not the modified worktree), run `git apply --check --verbose` against `50_add_susfs_in_gki-android12-5.10.patch`, capture stderr/stdout in a log. Failure is expected and helps locate differing APIs.
- Do a hunk-by-hunk inventory for `fs/namespace.c`, `fs/super.c`, `fs/namei.c`, `fs/stat.c`, `fs/statfs.c`, procfs, FUSE, `mm/memory.c`, SELinux and ARM64 thread flags.
- Use precise source and config checks to identify APIs missing from Motorola 4.19, rather than assuming a blanket v5.10 backport can compile.

### Stage 3 — Port SUSFS 2.3.0 coherently in small commits

1. Add shared definitions, enum/command values, public structs and prototypes; establish ABI tests and compile guards.
2. Port core `fs/susfs.c` with matching lifetime/synchronization behavior and feature configuration; handle FUSE compatibility explicitly.
3. Port VFS/mount management to **Motorola's `fs_context` / `vfs_create_mount()` architecture**, ensuring old 1.5.9 logic is removed/reconciled (never duplicate ID rewrite paths).
4. Port file and procfs hooks, stat/statfs, memory-management hooks, SELinux integration and BakaSU runtime calls with source-specific edits.
5. Fix all manual/SUSFS mode guards and inspect actual event-hook placement. Resolve all undefined/duplicate symbols and compile warnings.
6. Address BakaSU's upstream compile restriction only after the chosen, pinned integration has demonstrably compatible interfaces and a reviewer-approved explanation.

### Stage 4 — Validation ladder

1. Diff/whitespace checks, controlled patch application in a clean worktree.
2. Kconfig sanity (`CONFIG_KSU=y`, `CONFIG_KSU_SUSFS=y`, `CONFIG_KSU_MANUAL_HOOK=n`, appropriate `CONFIG_THREAD_INFO_IN_TASK` and `CONFIG_KALLSYMS_ALL` checks as required by current CI).
3. Build `Image dtbs modules` with the actual ARM64 Clang and vendor configs; capture full log and warning summary.
4. Static/ABI review: symbol references; hook compilation and call paths; mount-ID uniqueness/cleanup; error handling, locking/refcounts and buffer length checks.
5. Repack only with checksum-verified Axion 2.8 base `boot.img`, produce SHA-256 manifests and provenance information for every artifact.
6. **After separate approval and suitable recovery prep**, try **`fastboot boot`** of experimental image first, check boot, `adb`, modules, root, mounts, filesystem operation, app sandbox behavior and hardware. Review failure logs/possible reboot loops. Avoid permanent flashing until satisfactory results.

### Immediate Codex deliverables

Before writing kernel code, produce:

- `docs/SUSFS_2_3_API_MATRIX.md` (old/new BakaSU API and structures, exact file:line refs);
- `docs/SUSFS_2_3_PORT_PLAN.md` (file-by-file mapping, impact/risk, compile/test plan);
- a concise current-state list of discrepancies between ZIP snapshots and live repository; and
- a proposal for the **first small, reviewable patch** (no automatic branch push, boot image flash, or compatibility bypass).

---

## 8. Fast handoff prompt for Codex

Copy this into a **new Codex chat** with the kernel repo open. Codex should **read this entire file first**:

> Read `BACKPORT_HISTORY.md` in full before making edits. This project is the Motorola Moto G32 (`devon`) Linux 4.19.325 non-GKI kernel with working BakaSU on `devon-resukisu` and an experimental SUSFS 1.5.9 patch on `devon-resukisu-susfs`. We are deliberately backporting **SUSFS v2.3.0** (`ShirkNeko/susfs4ksu` `gki-android12-5.10`, SHA `82f08351fadb8b19dd6591bd14634d02ad832145`) to Motorola 4.19 while preserving `fs_context`/`vfs_create_mount()` and making it compatible with the **pinned** BakaSU revision from the failed build (reported `f4435f00`). Review all past failures, especially SUSFS 1.5.9 API mismatch, `CONFIG_KSU_MANUAL_HOOK` guards, input hook placement, mount-ID behavior, and unconditional `susfs_compat.mk` stop. **First work read-only**: verify branch, commits, dirty changes, source snapshots, and workflow; produce an exact file/function interface matrix and staged plan. **Do not** touch stable branch, bypass compatibility restrictions, rewrite version strings to fake compatibility, push, alter original boot image, or flash. Ask before any risky action.

---

## 9. Useful file/repo references

| Item | Location / notes |
|---|---|
| User repo | https://github.com/Ojas7112008/android_kernel_motorola_sm6225 |
| Working branch | `devon-resukisu` |
| SUSFS experimental branch | `devon-resukisu-susfs` |
| Workflow in repo | `.github/workflows/build-devon.yml` (verify latest content) |
| Existing device-side patch | `.github/patches/devon-susfs-4.19.patch` |
| Old SUSFS upstream/mirror | https://github.com/ShirkNeko/susfs4ksu (branch `kernel-4.19`; pinned old SHA above) |
| New SUSFS upstream/mirror | https://github.com/ShirkNeko/susfs4ksu (branch `gki-android12-5.10`; SHA above) |
| Original SUSFS project | https://gitlab.com/simonpunk/susfs4ksu |
| BakaSU | https://github.com/Baka-SU/BakaSU |
| BakaSU inline hook checker | `KernelSU/kernel/tools/inline_hook_check.mk` in action checkout; `bakasu/tools/inline_hook_check.mk` in source ZIP |
| BakaSU compatibility stop | `KernelSU/kernel/tools/susfs_compat.mk`; `bakasu/tools/susfs_compat.mk` in source ZIP |
| BakaSU SUSFS command dispatch | `bakasu/supercall/dispatch.c` in source ZIP |
| SUSFS 2.3 source reference | `$HOME/susfs-backport/susfs-5.10/kernel_patches/` on previous Codespace (may not exist on new PC) |
| SUSFS 2.3 integration patch | `kernel_patches/50_add_susfs_in_gki-android12-5.10.patch` |
| User-uploaded source snapshot | `susfs-backport-sources.zip` (separate; not automatically part of git repo) |
| User-uploaded diagnostic | `susfs-diagnostics.log` (separate) |
| Earlier comparison audit | `susfs_4_19_backport_audit.md` (separate) |
| Original ROM boot asset | GitHub release `axion-2.8-boot-base` / `axion28-devon-original-boot.img.zst` |

### Working tree caution

A freshly cloned repository will contain **only committed files**. If the Codespaces working tree is dirty, export or commit reviewed modifications on an **experimental** branch first, without touching the known-working branch. The local `susfs-backport` directory and uploaded ZIPs are not automatically synchronized by Git. Either copy those references to the new environment or instruct Codex to clone pinned refs itself.

### Explicit current-state checkpoint

At the end of the last available evidence, **we had not yet received** `susfs-23-compatibility.log`, and **no SUSFS 2.3.0 code modifications had been validated in a build**. The next task is **source-level audit and planning**, then a controlled port in small patches. Treat the goal as unfinished.

---

## 10. Documentation maintenance rule

As Codex proceeds, append dated entries to this file (or `docs/PORT_LOG.md`) recording: exact source SHAs; changes applied; commands, test logs and exit codes; compiler/link errors; why a fix was chosen; negative results; and whether each change passed **source review**, **compile**, **temporary boot**, or **runtime validation**. Never mark an unexecuted test as passed. This prevents repeating the earlier trial-and-error work in future sessions.
