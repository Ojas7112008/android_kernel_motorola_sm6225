# Android Clang integrity

Both variants use official Clang r547379 from immutable Android commit
`9916fb51ccb914d62d35ad9a7b9b21d2ef046928`, directory `clang-r547379`.
[Official source tree](https://android.googlesource.com/platform/prebuilts/clang/host/linux-x86/+/9916fb51ccb914d62d35ad9a7b9b21d2ef046928/clang-r547379/).

The first cloud runs stopped at a compiler archive checksum mismatch before
any kernel compilation. Two independent downloads of a small directory from
that same immutable commit produced different tar mtimes and transport hashes,
while all contained file SHA256 hashes matched. Gitiles archive bytes are
therefore unsuitable as the sole reproducibility pin. The old compressed
archive hash remains historical provenance.

The installer now verifies the entire extracted compiler before installing or
executing it. Filenames, file bytes, executable modes and symlink targets must
match both the official Git SHA1 tree and a separately pinned deterministic
SHA256 content tree. It also keeps the seven original executable SHA256 pins.
The SHA256 tree uses Git object framing and ordering with 32-byte child IDs;
the regression suite compares it against Git's SHA256 object format.

- Official Git tree: `17fee43bde5bc5860dc1d04be5ead3bd6e6f91f0`
- Deterministic SHA256 content tree: `ed38c5043e6b711d1eb8f42432556a770a3b68dfc48e1a9cc7025258d246bf3f`

These were derived from the previously archive-verified local compiler and
independently matched to the official pinned Git tree. Every existing install
is checked again before use. Download transport hashes are recorded beside
the installed compiler for diagnostics. The archive is extracted only into a
temporary directory with path/link/type checks; a mismatching compiler is
never executed. This changes compiler transport handling only; kernel source,
BakaSU compatibility and version checks, CFI/LTO and module checks are unchanged.

`python3 -B scripts/ci/test_toolchain_integrity.py` verifies Git agreement,
timestamp independence, and rejection of changed library contents, executable
modes, executable fingerprints and archive-only fixture corruption.
