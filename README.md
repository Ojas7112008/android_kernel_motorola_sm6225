# Moto G32 (devon): BakaSU and SUSFS builds

This fork follows LineageOS `lineage-23.2` at `c65462bf08da2a8900b98e705bea376de7f7f19c` (Linux 4.19.325, non-GKI).

| Variant | Source branch | Manual Actions workflow | BakaSU commit |
| --- | --- | --- | --- |
| BakaSU only | `lineage-23.2-bakasu` | Devon BakaSU (non-SUSFS) | `d7260ca1e5c128061122924d0d48754e361e135c` |
| BakaSU + SUSFS 2.3 | `lineage-23.2` (default) | Devon BakaSU + SUSFS 2.3 | `e1682a2831142e68fa30bab97d5690cf90b75119` plus reviewed Devon provider patch |

Open **Actions**, select the desired workflow, then **Run workflow** from the default branch. Each workflow explicitly checks out the source branch listed above. Both are manual-only and upload `Image`, DTBs, modules, configuration, checksums and provenance/logs for seven days. They do not repack a ROM boot image, operate on a device or publish a release.

The SUSFS workflow selects all eight reviewed features: SUS_PATH, SUS_MOUNT, SUS_KSTAT, SPOOF_UNAME, HIDE_KSU_SUSFS_SYMBOLS, SPOOF_CMDLINE_OR_BOOTCONFIG, OPEN_REDIRECT and SUS_MAP. The 5.10 reference is SUSFS commit `82f08351fadb8b19dd6591bd14634d02ad832145`. AVC log spoofing returns `EOPNOTSUPP`. Native BakaSU version and compatibility checks, SELinux enforcement, Clang CFI/LTO and module versioning remain enabled.

For local Linux/WSL builds, see [docs/DEVON_BUILDS.md](docs/DEVON_BUILDS.md). The official Android Clang r547379 archive and executable hashes are pinned; changing a branch or dependency requires deliberate review, not a floating update.

Earlier source revisions passed kernel builds and limited device boot/root/module/mount-ID checks. This publication moves the port onto the newer clean Lineage base. A successful build does not establish safe booting, hiding-rule correctness or long-term stability. Match the target ROM's vendor module ABI and device trees before separately authorized device testing. No boot images or personal device logs are stored in this repository.

Upstream credits: LineageOS/Motorola and Linux contributors; [BakaSU](https://github.com/Baka-SU/BakaSU); [SUSFS](https://gitlab.com/simonpunk/susfs4ksu), mirrored by [ShirkNeko](https://github.com/ShirkNeko/susfs4ksu). Existing source licenses and copyright notices apply.
