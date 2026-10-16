# SUSFS 2.3.0 backport provenance

**Historical checkpoint:** Source selection and SELinux ownership have been superseded by the [automatic-hook checkpoint](SUSFS_2_3_SELINUX_AUTOHOOKS.md). Original source pins/results below are preserved as historical evidence.

Date: 2026-10-13 (Asia/Calcutta).

This is a documentation checkpoint for the Motorola Moto G32 (devon), Linux 4.19.325 non-GKI backport. No SUSFS 2.3.0 kernel implementation is present at this checkpoint. Source review, compilation, boot and runtime validation are separate gates.

## Approved scope and protected baseline

The user approved the proposed first change: create an isolated worktree and prepare documentation/provenance. This checkpoint changes documentation only. Kernel sources, the legacy patch, workflows and BakaSU compatibility checks remain unchanged. No build, CI dispatch, push, release, boot-image operation or device operation was performed.

The Codex app create_worktree tool returned "create_worktree received invalid arguments." Native WSL Git was used instead; the checkout is a normal Git worktree, not an app-managed attachment.

| Input | Pinned value |
|---|---|
| Kernel experimental baseline | 18bd9752ada79090ce11326662eaaa9f33717728 |
| Original experimental branch | devon-resukisu-susfs |
| Documentation branch | susfs-2.3-backport |
| Protected remote branch | devon-resukisu |
| Protected remote SHA | 80f0ee4d1ec68838d25c2efeb955d31e90f85dae |
| SUSFS 1.5.9 baseline | 1f764cfb8ddc3b19d7624731051d1000d629175b |
| SUSFS 2.3.0 reference | 82f08351fadb8b19dd6591bd14634d02ad832145 |
| BakaSU reference | f4435f007b16f61705089ff982e3f981a6017979 |

The original checkout is /home/shinji/projects/android_kernel_motorola_sm6225.
The new worktree is /home/shinji/projects/android_kernel_motorola_sm6225-susfs-2.3-backport.
Both are accessible through the Ubuntu WSL filesystem from Windows.

Remote branch SHAs were verified by git ls-remote during the initial read-only audit. The supplied BakaSU files match the resolved f4435f00 commit, but the original CI artifacts are absent: this verifies snapshot identity, not the provenance or completeness of the reported successful manual build.

## Preservation and source state

BACKPORT_HISTORY.md was copied byte-for-byte from the original checkout. Its original SHA-256 is 2d3eff3723b501f3f55a428adfd0fb2f03134680f08b2da5f62297b53f928613. Subsequent entries belong in PORT_LOG.md so this input remains immutable.

The new worktree also preserves the supplied ZIP, diagnostic log and all extracted references under .backport-context/. These references and generated audit logs are ignored local evidence, not proposed source changes. A new clone will require this context to be transferred separately. The input manifest records all 125 evidence files by size and SHA-256; a missing input must not be treated as a passing verification.

| Local evidence | Finding |
|---|---|
| .github/patches/devon-susfs-4.19.patch | 117351 bytes; 23 paths; identical to ZIP copy |
| Legacy patch SHA-256 | 63884a4f81b3efaae3697673e543b41b0118c6b259c7de602d1dd8ce5b27c5e5 |
| ZIP | 120 files; all extracted contents match |
| Motorola snapshots | 15 source files exactly match an in-memory reconstruction of the legacy patch against the pinned checkout |
| BakaSU snapshots | All 99 supplied files match Git blob IDs in the pinned upstream tree |
| SUSFS 2.3.0 snapshots | All five supplied files match Git blob IDs in the pinned upstream tree |
| Legacy core/header definitions | The 1.5.9 header matches upstream exactly; core and definitions differ only in blank lines/trailing whitespace, as inspected in the initial audit |
| Live kernel source | SUSFS patch is unapplied; no KernelSU checkout or build output exists |
| Historical handoff | Originally untracked; preserved here as proposed documentation |

The experimental branch changes only the workflow and patch relative to the protected branch. Missing applied fs/include/kernel edits in a fresh checkout are expected in this patch-based CI design. No missing Codespace edit was found within the supplied Motorola snapshots. Unsupplied files cannot be reconstructed or ruled out.

Missing historical material includes the earlier susfs_4_19_backport_audit.md, complete failed Actions logs/artifacts, and persistent reference clones with Git metadata. The selected BakaSU snapshot omits Kbuild, setup.sh and other files required by a complete checkout. These two build files were inspected from the pinned upstream revision without executing them.

## Validation actually executed

Checks below were repeated in the isolated worktree before documentation was written. Logs identify commands and exit codes. No patch was applied to kernel source.

| Check | Exit | Interpretation |
|---|---:|---|
| git apply --check --verbose --whitespace=error-all .github/patches/devon-susfs-4.19.patch | 0 | Legacy patch applies cleanly; no strict whitespace errors |
| git apply --check --verbose .backport-context/sources/susfs_2_3/50_add_susfs_in_gki-android12-5.10.patch | 1 | Expected incompatibility; 19 of 24 paths fail |
| git diff --check on unchanged baseline | 0 | No tracked baseline differences |
| ZIP and snapshot byte comparisons | Passed | All 120 entries preserved |
| In-memory legacy patch reconstruction | Passed | All 15 Motorola source snapshots reproduced |
| Upstream Git blob comparisons | Passed | 99 BakaSU and five SUSFS reference files match pinned trees |

The 5.10 patch fails on fs/Makefile, fs/exec.c, fs/namei.c, fs/namespace.c, fs/notify/fdinfo.c, fs/open.c, fs/proc/base.c, fs/proc/bootconfig.c, fs/proc/fd.c, fs/proc/task_mmu.c, fs/readdir.c, fs/stat.c, fs/statfs.c, fs/super.c, kernel/kallsyms.c, kernel/reboot.c, kernel/sys.c, mm/memory.c and security/selinux/avc.c. Bootconfig is absent; the other 18 paths have context failures.

The five paths with no reported file failure are drivers/input/input.c, fs/proc_namespace.c, fs/read_write.c, security/selinux/hooks.c and security/selinux/selinuxfs.c. Applicability does not establish compatible signatures or correct behavior.

Local logs and comparison records:

- .backport-context/audit/legacy-patch-check.log
- .backport-context/audit/susfs-23-compatibility.log (new audit output, not a recovered Codespace log)
- .backport-context/audit/baseline-diff-check.log
- .backport-context/audit/source-state.log
- .backport-context/audit/snapshot-comparison.json
- .backport-context/audit/upstream-blob-verification.json

## Build restriction and confidence limits

Pinned BakaSU Kbuild includes tools/susfs_compat.mk in SUSFS mode. The fragment unconditionally stops with "Please wait susfs update." Its evaluation precedes SUSFS version reporting. Upstream main resolved to the same revision during the initial audit. No supported newer pairing was established.

The prior non-SUSFS temporary boot/root/hardware results are user-reported historical evidence. There is no locally verified SUSFS compile, link, boot or runtime result. The current audit does not certify the legacy patch, reference patch or chosen BakaSU revision as safe.

Authoritative references:

- [Kernel baseline](https://github.com/Ojas7112008/android_kernel_motorola_sm6225/commit/18bd9752ada79090ce11326662eaaa9f33717728)
- [BakaSU restriction](https://github.com/Baka-SU/BakaSU/blob/f4435f007b16f61705089ff982e3f981a6017979/kernel/tools/susfs_compat.mk)
- [BakaSU Kbuild](https://github.com/Baka-SU/BakaSU/blob/f4435f007b16f61705089ff982e3f981a6017979/kernel/Kbuild)
- [SUSFS reference tree](https://github.com/ShirkNeko/susfs4ksu/tree/82f08351fadb8b19dd6591bd14634d02ad832145/kernel_patches)

See [API matrix](SUSFS_2_3_API_MATRIX.md), [port plan](SUSFS_2_3_PORT_PLAN.md), [input manifest](SUSFS_2_3_INPUT_MANIFEST.json), [port log](PORT_LOG.md), and the [complete handoff](../BACKPORT_HISTORY.md).
