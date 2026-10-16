# Reproducible Devon kernel builds

Use a Linux filesystem without spaces (WSL ext4 is supported), Python 3.11 or newer, and the build dependencies listed in the workflows. Allow about 20 GB free for source, the 4 GB compiler and one complete build. Full kernel history and full BakaSU history are required. The compiler installer verifies every file, executable mode and symlink against immutable Git/SHA256 tree pins, plus selected executable SHA256 hashes.

## BakaSU only

```sh
git clone --branch lineage-23.2-bakasu --recurse-submodules https://github.com/Ojas7112008/android_kernel_motorola_sm6225.git
cd android_kernel_motorola_sm6225
python3 scripts/ci/install_toolchain.py
bash scripts/devon/build-bakasu.sh
```

The submodule uses the pinned native manual-hook provider. Build outputs are in `artifacts/`. The top-level `build.sh` is a historical optional ROM-specific repacking helper; the CI build invokes the kernel-only driver above. A newer BakaSU revision is a new validation target.

## BakaSU + SUSFS 2.3

```sh
git clone --branch lineage-23.2 https://github.com/Ojas7112008/android_kernel_motorola_sm6225.git
cd android_kernel_motorola_sm6225
python3 -B scripts/susfs/prepare_test_references.py
python3 -B scripts/susfs/run_host_checks.py
bash build-susfs-test.sh --prepare --profile all
# To compile after source/config checks:
bash build-susfs-test.sh --build --profile all --jobs 4
```

The builder downloads and verifies pinned dependencies automatically; `.backport-context/` remains ignored. It applies the checked Devon 4.19 SELinux/provider adaptation to the exact BakaSU revision and evaluates BakaSU's native Kbuild compatibility checks. Source hashes reject drift. All-feature outputs and logs appear under `artifacts/susfs-features-test/<run>/`. The core-only default remains available for incremental debugging; CI explicitly selects `all`.

After preparation or compilation, run `python3 -B scripts/susfs/test_build_preparation.py <artifact-directory> --profile all` to exercise native rejection paths and source/config/compiler integrity checks. Host fixtures model interfaces, locking and error paths; they cannot establish native races or boot behavior. Older checkpoint tests remain for historical comparisons and may rely on their original staged headers; the maintained CI suite is explicitly listed in `scripts/susfs/run_host_checks.py`.

The clean Lineage base and all production backport files are pinned in `scripts/susfs/build-pins.json`. `scripts/susfs/fixtures/legacy-kernel/` contains hashed pre-fix negative controls from the old branch, never build inputs. The old fork is not needed to build. Keep original ROM recovery images outside source control. Device operations require separate review and authorization.


## Workflow output choice

In Actions, select either workflow and choose `kernel` (default) or `boot.img`
in **Output file**. A successful run uploads exactly one payload and one
checksum file:

| Choice | Download contents |
| --- | --- |
| `kernel` | `Image`, `Image.sha256` |
| `boot.img` | `boot.img`, `boot.img.sha256` |

Raw output is byte-identical to the compiled ARM64 Image. Boot output uses only
the exact original Axion 2.8 96 MiB Android-v3 boot image, SHA256
`1632d107116564f6130b8633c5c21b09261009a03712e086dd22cb9e71edc4de`.
The base is downloaded only for `boot.img`; both compressed and decompressed
SHA256/size pins are checked. The ramdisk, header settings and AVB flags remain
unchanged; only kernel placement, component sizes and the existing unsigned
AVB boot hash are updated. No module archive is inserted. This packaging does
not establish vendor module compatibility, safe booting or runtime correctness.

Internal DTBs, modules, configuration, source evidence and logs are still built
and checked on the runner, but only `dist/` is uploaded after all checks pass.
Failure logs remain available in the Actions job; no partial image artifact
is uploaded. The original boot resource is separate from the result artifact
and is never put in Git history. Its exact URL and hashes are recorded in
`scripts/ci/boot-base.json`.

For local packaging after a successful build, use:

```sh
python3 -B scripts/ci/package_output.py --variant susfs --format kernel
# Or, for the supplied boot base:
python3 -B scripts/ci/package_output.py --variant susfs --format boot.img --base /path/to/original/boot.img
```

Use `--variant bakasu` on the BakaSU-only branch. Optional `--artifacts` selects
an explicit build directory. Existing output directories are refused to avoid
overwriting previous files. Verify the chosen payload with
`sha256sum -c Image.sha256` or `sha256sum -c boot.img.sha256`.
