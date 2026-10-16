# Reproducible Devon kernel builds

Use a Linux filesystem without spaces (WSL ext4 is supported), Python 3.11 or newer, and the build dependencies listed in the workflows. Allow about 20 GB free for source, the 4 GB compiler and one complete build. Full kernel history and full BakaSU history are required. The compiler installer verifies both the archive and selected executables.

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

The clean Lineage base and all production backport files are pinned in `scripts/susfs/build-pins.json`. `scripts/susfs/fixtures/legacy-kernel/` contains hashed pre-fix negative controls from the old branch, never build inputs. The old fork is not needed to build. Keep original ROM recovery images outside source control. Packaging and device operations require separate review and authorization.
