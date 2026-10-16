# SUSFS 2.3.0 reboot/input checkpoint

**Checkpoint extension:** The [core foundation](SUSFS_2_3_CORE_FOUNDATION.md) adds core queries, initialization and the 4.19 monitor. Earlier next-boundary statements below describe the preceding checkpoint.

Date: 2026-10-14 (Asia/Calcutta). Source and host validation only; no ARM64 build or device operation.

The user authorized this next checkpoint on the isolated susfs-2.3-backport branch. The protected devon-resukisu ref, original checkout and immutable inputs remain unchanged. Ten previous inputs/results are archived under .backport-context/audit/reboot-input/prior-checkpoint. The exact source pins and 16-file provider scope from [source-pins.json](../scripts/susfs/source-pins.json) are retained. This extends the [init.rc checkpoint](SUSFS_2_3_INIT_RC.md).

## Reboot transport

[kernel/reboot.c](../kernel/reboot.c) calls the pinned BakaSU reboot provider in SUSFS mode and retains the existing manual mode. The [shared header](../include/linux/ksu_susfs.h) declares its exact int/int/unsigned-int/void-user-pointer-reference signature. The syscall supplies the address of its local argument; the provider sees the userspace payload as *arg. No additional userspace pointer indirection is introduced.

The provider is unchanged from e1682a2. It rejects a non-KSU first magic, handles driver installation separately, requires real UID zero for extensions, lets the toolkit select its own magic, and dispatches SUSFS_MAGIC to the existing SUSFS dispatcher. Installation remains available before the native CAP_SYS_BOOT check, as intended by the upstream transport. The ordinary reboot capability, magic, PID-namespace, power-off fallback, switch, user-copy and mutex paths are preserved.

The syscall deliberately ignores the provider return as pinned userspace expects: driver installation reads its returned FD buffer; SUSFS reads command payloads and their error fields. A nonprivileged installation can fill the FD buffer while reboot returns EPERM; a capable KSU transport caller normally receives EINVAL because the KSU magic differs from the native reboot magic. Neither return proves that a command worked. Real allocation/scoped FD permissions and SUSFS payload handlers are not validated by this checkpoint.

## Input and safe-mode cutoff

[drivers/input/input.c](../drivers/input/input.c) invokes BakaSU only after native disposition accepts an EV_KEY/KEY_VOLUMEDOWN/value-1 transition. Native state filters duplicate down events; value 2 repeats, releases, unsupported keys and other events do not count. The hook receives copies, so provider writes or errors cannot alter the native event already classified. There remains exactly one native entropy contribution per accepted nonsync event and no added entropy call.

The target provider protects its counter and stopped state with one IRQ-safe spinlock. The counter saturates at the existing threshold of three. Separate devices cannot lose increments or race an unsynchronized counter. Input holds its device event lock first, then the provider lock. Stopping takes only the provider lock, publishes the permanent cutoff, releases it, and lets one caller disable the static key. A query freezes the counter before reading it under the same lock, so later events cannot change the safe-mode decision.

No input callback stops the static key, schedules work, allocates, performs VFS work or sleeps. Existing post-fs-data and driver query paths stop it in process context; jump-label patching occurs after releasing the spinlock. The built-in target has no unload/reset path that reopens the cutoff. Original non-target/manual/tracepoint provider paths are retained. On real hardware, contact bounce that the driver reports as separate down/up transitions still appears as distinct presses; this checkpoint does not add device-specific debounce.

## Executed checks

Run [test_reboot_input.py](../scripts/susfs/test_reboot_input.py). It strictly reconstructs/reverses the authoritative provider patch against e1682a2 and extracts actual native reboot/input and provider function bodies. The [reboot fixture](../scripts/susfs/test_reboot.c) records all reboot actions; it never reboots a machine. The [input fixture](../scripts/susfs/test_input.c) uses host locks to model IRQ nesting and verifies static-key disable occurs with no simulated atomic lock held.

Eight ASan/UBSan configurations pass: reboot and input each with hooks off, existing manual mode, SUSFS with KSU_COMPAT_USE_STATIC_KEY absent, and SUSFS with it defined. The SUSFS input enable key itself is unconditional in the pinned SUSFS branch. Wrong reboot argument indirection and wrong input value type both fail compilation against the shared header.

Checks cover native reboot capability/magic order, all four native second magics, namespace refusal, CAD/restart/restart2/halt/power-off/kexec/hibernate behavior, user-copy errors, FD install/failure/copy-out cleanup, real-UID extension gating, toolkit/SUSFS routing, payload-reference identity and preservation of native transport errno. FD allocation, extension dispatch, user copies and privileged actions are mock boundaries. A returned restart2 mock retains the native string-copy length; actual restart is never executed.

Input checks execute native event support/disposition, state transitions, delivery/flush and entropy placement. Tests cover duplicate/repeat/release/unsupported events, early null event buffers, normalization from a mocked ABS helper, defensive argument copies, repeated queries, the three-press saturation, post-fs-data-style cutoff, eight concurrent devices and event/stop races. A pinned-provider negative control reproduces one press plus two repeats incorrectly satisfying its raw three-event threshold. Host pthread mutexes do not establish real spinlock, IRQ, lockdep or jump-label safety.

The unchanged upstream inline-hook checker now exits zero with all seven names found; its manual-guard warnings remain visible. The automatic SELinux runner now requires that observed success rather than the previous expected missing-input failure. The upstream compatibility fragment, non-GKI SELinux rejection and invalid-mode failures remain enforced. A name-based checker is not a complete source/ABI/build validation.

Evidence is under .backport-context/audit/reboot-input. Prior SELinux, UID/task, access/stat and init.rc source/host regressions are run against the updated shared header/provider and retain their actual-source negative controls. All 25 current positive configurations pass. The final audit verifies 125 immutable inputs, branch/ref preservation, ten archived prior inputs, recorded input hashes and 81 local document links. The 69-line kernel-stage diff has zero checkpatch findings; the full external provider exits 1 with 299 tab-indentation errors and 817 warnings, retained as style findings. Exact results are recorded in [PORT_LOG.md](PORT_LOG.md).

## Next boundary

Finish remaining startup consumers, then port SUSFS core, payload ABI and fsnotify lifetime before mount-ID/namespace and filesystem/procfs features. The legacy workflow still applies 1.5.9 and floating BakaSU. A pinned source/config/toolchain workflow and real ARM64 builds come after those source gates. Runtime hook installation, seccomp, FD permission scope, input IRQ behavior, CFI/LTO and actual init/ROM behavior remain unvalidated. No successful host test establishes a bootable or safe kernel.
