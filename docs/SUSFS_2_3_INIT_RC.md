# Devon read/init.rc and FD-stat checkpoint

**Checkpoint extension:** The [reboot/input checkpoint](SUSFS_2_3_REBOOT_INPUT.md) now supplies the SUSFS reboot route and accepted input press hook. The unchanged inline name checker passes with manual-guard warnings; earlier missing-input/manual-only findings below describe the prior checkpoint. Core/payload ABI and ARM64 build remain incomplete.


Date: 2026-10-14 (Asia/Calcutta). This extends the [access/stat checkpoint](SUSFS_2_3_ACCESS_STAT.md), following the user's approval to continue. Source and host checks pass; full ARM64 compilation and kernel/device behavior remain unvalidated.

## Source and boundaries

Retained kernel 18bd9752ada79090ce11326662eaaa9f33717728, BakaSU e1682a2831142e68fa30bab97d5690cf90b75119 plus the existing devon policy/SID-hash adaptation, and SUSFS 82f08351fadb8b19dd6591bd14634d02ad832145. The authoritative external provider patch remains 16 files, with this stage extending runtime/ksud_integration.c. Root changes extend fs/stat.c, add held-file read/iterator/seek hooks to fs/read_write.c, add final-close cleanup in fs/file_table.c and expand the shared typed header. No native structure layout is changed.

Prior source, provider, header, pins and passing results are preserved under .backport-context/audit/init-rc/prior-checkpoint. The original handoff/125 inputs, legacy patch/workflow, reference repositories and protected devon-resukisu branch remain unchanged. No version or upstream check is bypassed. No commit, push, CI dispatch, image, release, flash or device operation.

## Read and size contract

The pinned provider uses a global one-shot file-operation proxy and global append cursors. Its module data is freed after reading, which also clears the module length used by subsequent fstat calls. Its user-copy error can return zero before logical EOF, and append operations do not advance the native position. The native Motorola replace_fops macro is documented for open-time use; this stage uses inline consumers and a per-file sidecar instead of replacing a published f_op or private_data.

Only PID 1 with real UID 0, the actual init SELinux context, ready KSU credentials, comm init, a regular exact /init.rc or /system/etc/init/hw/init.rc path, and a live opened readable non-writable non-stream non-direct file receives a view. The upstream init.rc enable flag gates new views; existing views retain their size/data contract after that flag is disabled. Native argument, mode, user-buffer, read-permission and getattr checks run first. Failed native fstat/LSM calls never alter size or initialize a view. FD size is adjusted only when STATX_SIZE is actually valid in the returned result mask. Nonmatching files/processes retain native behavior.

Each view belongs to the held struct file, captures its base size, and references an immutable boot-tail cache containing the unchanged KERNEL_SU_RC followed by optional module RC bytes. Repeated fstat calls report the same logical size before and after reading. Reads preserve native prefix operations, cap them at the captured boundary, and copy the tail using the logical position. Partial user/iterator copies advance by the bytes actually delivered; a complete copy fault returns EFAULT rather than false EOF. Zero-length calls initialize nothing. Short/error prefix reads retain native results; unexpected EOF before the captured prefix ends returns EIO. Size changes after view creation return ESTALE instead of silently moving the tail boundary.

Synchronous read/read_iter/readv/pread paths share this contract. Native iterator counts are restored after a capped prefix read, accounting for the actual bytes consumed. SEEK_SET/CUR keep the original file operation; SEEK_END uses the logical size with overflow checks. Rewind permits a complete reread. A captured view follows the held file across pathname renames; later eligibility still requires its authenticated caller and open mode. Separate opens have separate base/position lifetimes, and descriptor aliases share the native file/position. Hooks use the file already held by the native caller, avoiding a second fd lookup after successful validation.

The staged view rejects asynchronous iterator reads through vfs_iocb_iter_read, nonzero readv flags and SEEK_DATA/HOLE with EOPNOTSUPP for eligible init.rc files. Direct opens are not overlaid. Pathname stat, mmap, splice/sendfile and other inode-backed interfaces retain the physical file; this is an FD/read overlay, not a general virtual filesystem or a backported statx mount-ID ABI. Real init/ROM consumers must be checked against these supported paths before a boot is considered. Native/compat stat copyout and syscall layouts remain unchanged, but their actual target compilation remains a separate gate.

## Cache and lifetime

Module discovery preserves the watchdog/default paths. Missing or non-directory path components at both paths and the explicit custom-RC-disable option produce the static tail only. Nonblocking opens permit type validation without waiting on a FIFO. Other open/type/read errors are reported; failed loads remain retryable. Module size is bounded to 1 MiB, negative/oversized sizes are rejected, and short reads are retried until the captured extent is complete. Premature EOF/invalid progress is rejected. Credentials, temporary buffers and module files are unwound on every tested failure.

A separate cache mutex serializes discovery/publication. Filesystem I/O occurs outside the view/close mutex, which guards only sidecar lookup/publication/removal. Racing first users converge on one view; losing candidates are freed. The immutable tail is deliberately retained until reboot under the target's enforced built-in KSU configuration. Per-file state is removed in final __fput after eventpoll cleanup and before native release/free; no file reference is held by the sidecar, so no reference cycle is introduced. The borrow contract relies on the native caller retaining the file throughout each hook/read. Real native lifetime and lock ordering still require kernel validation.

The original non-target/manual/tracepoint proxy remains behind its original configuration path. Legacy provider entry points are retained; the new root consumers use checked held-file APIs and propagate their errors. This stage does not claim to repair or validate the protected manual build's complete runtime behavior.

## Executed validation

```sh
python3 -B scripts/susfs/test_init_rc.py
python3 -B scripts/susfs/test_access_stat.py
python3 -B scripts/susfs/test_setresuid_task_state.py
python3 -B scripts/susfs/test_selinux_autohooks.py
```

The new runner strictly reconstructs/reverse-checks the 16-file provider and extracts the target provider, actual KERNEL_SU_RC, native vfs_statx_fd/vfs_llseek, vfs_read/new_sync_read/__vfs_read and synchronous/asynchronous iterator consumers. It asserts exact final-close placement in native __fput. Four ASan/UBSan configurations pass: hooks off, manual native path and SUSFS static keys off/on.

Host checks cover native error order and valid-size masks, byte-for-byte prefix/static/module output, repeated fstat/EOF, rewind/SEEK_END/overflow, partial/failing copies, scatter buffers, iterator count restoration, separate opens, close/object reuse, absent/disabled/invalid/oversized/failed module data, allocation failure/retry, changed base extent, unexpected EOF, credential/process/mode gates and eight concurrent first users. Module IO is mocked through the extracted native read path under overridden credentials. The mock module close performs immediate cleanup, exercising the lock separation more strictly than native deferred __fput. Native operation tables and private_data remain unchanged.

An actual pinned-proxy negative control reproduces false EOF on user-copy failure, unchanged append position, shrinking fstat size after module consumption and size inflation based on a spoofed init comm. Module discovery for this negative control is mocked to isolate the actual proxy/fstat behavior. Positive native credential/VFS/file lifetime and host list/mutex/iterator primitives are modeled, not real kernel mechanisms. Async, mmap/splice, target ABI, CFI/LTO, live policy and device behavior are not certified by these tests.

Initial harness failures were an overly broad file_free substring assertion and missing/conflicting host GNU/Linux type/iterator declarations; corrected them without changing native kernel code or compatibility checks. The host signedness warning exception is retained for extracted unchanged kernel code, while incompatible-pointer diagnostics remain errors. Logs, source hashes, negative control, final-close extraction and final integrity/style evidence are under .backport-context/audit/init-rc.

The unchanged inline checker still exits 2, now at the missing input handler. It finds the reboot name under its existing manual-only guard; that name match does not establish the SUSFS command route. Both reboot and input remain next implementation work.

The final audit verifies all 125 immutable inputs, the original checkout/protected ref, three clean pinned repositories, nine archived previous inputs, current result input hashes, unchanged exec ownership source and 67 local document links. The new 186-line kernel-stage diff passes checkpatch with zero errors/warnings. The external provider exits 1 with 293 tab-indentation errors and 790 warnings, reflecting BakaSU formatting and remaining style findings; it is not represented as a clean global checkpatch result. Reconstructed-source whitespace passes.

## Next boundary

Adapt reboot command transport and input hooks, then finish remaining startup consumers, SUSFS core/payload ABI/fsnotify, mount-ID/namespace ownership and filesystem/procfs features. The legacy workflow still applies 1.5.9 and floating BakaSU and is not a build path for this source. Only after replacing it with the exact validated source/config/toolchain inputs should the real ARM64 build matrix run, followed by independently authorized device gates.
