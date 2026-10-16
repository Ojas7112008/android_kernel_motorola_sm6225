#!/usr/bin/env python3
"""Actual guarded namei functions and whole Motorola readdir under host VFS models.

No kernel object, native concurrent dcache/RCU walk, FUSE daemon or device is run.
Production SUS_PATH remains rejected until its remaining coverage gaps are resolved.
"""
import json
from pathlib import Path
import re
from kernel_baseline import kernel_source
import subprocess
import sys
sys.dont_write_bytecode = True
from test_selinux_adapter import function, run, digest
from test_setresuid_task_state import section
from source_guards import without_feature_guards

NAMES = ["path_put_conditional", "path_to_nameidata", "complete_walk", "lookup_dcache", "__lookup_hash",
         "lookup_fast", "__lookup_slow", "lookup_slow", "may_lookup", "step_into", "walk_component", "link_path_walk",
         "open_to_namei_flags", "atomic_open", "lookup_open"]


def without_hooks(text):
    return without_feature_guards(text, ['CONFIG_KSU_SUSFS_SUS_PATH', 'CONFIG_KSU_SUSFS_OPEN_REDIRECT'])


def native_macro(source, name):
    lines = source.splitlines(True)
    start = next(i for i, line in enumerate(lines) if re.match(r"#define\s+"+name+r"(?:\(|\s)", line))
    end = start
    while lines[end].rstrip().endswith("\\"): end += 1
    return "".join(lines[start:end+1])


def prepare_fuse_index(root, out):
    native_list = (root/"include/linux/list.h").read_text()
    native_hash = (root/"include/linux/hashtable.h").read_text()
    code = "#pragma once\n"
    for name in ["INIT_HLIST_NODE", "hlist_unhashed", "__hlist_del", "hlist_del_init", "hlist_add_head"]:
        code += function(native_list, name)
    for name in ["hlist_entry", "hlist_entry_safe", "hlist_for_each_entry"]:
        code += native_macro(native_list, name)
    for name in ["DEFINE_HASHTABLE", "HASH_SIZE", "HASH_BITS", "hash_min", "hash_add", "hash_for_each_possible"]:
        code += native_macro(native_hash, name)
    code += function(native_hash, "hash_del")
    (out/"native_fuse_hash.h").write_text(code)
    (out/"fuse_index_under_test.h").write_bytes((root/"fs/susfs_fuse.c").read_bytes())


def prepare(root, out):
    out.mkdir(parents=True, exist_ok=True)
    prior = root / ".backport-context/audit/path-consumers/prior-checkpoint"
    for file in ["fs/namei.c", "fs/readdir.c"]:
        assert without_hooks((root/file).read_text()) == kernel_source(root, file), file
    # Fundamental cache allocation, native permissions and module layouts stay intact.
    for file in ["fs/dcache.c", "include/linux/fs.h", "include/linux/dcache.h", "include/linux/mount.h"]:
        assert (root/file).read_bytes() == kernel_source(root, file).encode()
    namei = (root / "fs/namei.c").read_text()
    for name in ["inode_permission2", "may_o_create", "may_open", "atomic_open"]:
        original = kernel_source(root, 'fs/namei.c')
        assert function(namei, name) == function(original, name)
    starts = {
        "path_to_nameidata": "static inline void path_to_nameidata(",
        "may_lookup": "static inline int may_lookup(",
        "step_into": "static inline int step_into(",
    }
    code = "#ifdef CONFIG_KSU_SUSFS_SUS_PATH\n" + native_macro(namei, "SUSFS_LOOKUP_METADATA") + "#endif\n"
    for name in NAMES:
        code += section(namei, starts[name]) if name in starts else function(namei, name)
    (out / "namei_under_test.h").write_text(code)
    (out / "readdir_under_test.h").write_bytes((root / "fs/readdir.c").read_bytes())
    (out / "dcache_hash.h").write_text(function((root / "fs/dcache.c").read_text(), "d_hash_and_lookup"))
    (out / "inode_predicate.h").write_text(function((root / "fs/susfs_path.c").read_text(), "susfs_is_inode_sus_path"))
    (out / "native_backing_inode.h").write_text(function((root / "include/linux/dcache.h").read_text(), "d_backing_inode"))
    inc = out / "linux"
    inc.mkdir(exist_ok=True)
    for name in ["stddef.h", "kernel.h", "export.h", "time.h", "mm.h", "errno.h", "stat.h", "file.h", "fs.h",
                 "fsnotify.h", "dirent.h", "security.h", "syscalls.h", "unistd.h", "compat.h", "uaccess.h", "magic.h"]:
        (inc / name).write_text("/* Types and VFS models declared in the fixture prefix. */\n")
    (inc / "errno.h").write_text("#include_next <linux/errno.h>\n")
    # Full production shared headers; other payload features remain disabled.
    for name in ["susfs.h", "susfs_def.h", "susfs_fuse.h", "ksu_susfs.h"]:
        (inc / name).write_bytes((root / "include/linux" / name).read_bytes())
    for name in ["cred.h", "types.h", "string.h", "bits.h", "thread_info.h", "uidgid.h", "utsname.h", "hashtable.h", "path.h", "statfs.h", "sched.h", "list.h", "spinlock.h"]:
        (inc / name).write_text("/* Host prefix supplies types and flags. */\n")
    (inc / "version.h").write_text("#define KERNEL_VERSION(a,b,c) (((a)<<16)+((b)<<8)+(c))\n#define LINUX_VERSION_CODE KERNEL_VERSION(4,19,255)\n")
    prepare_fuse_index(root, out)


def main():
    root = Path(__file__).resolve().parents[2]
    out = root / ".backport-context/audit/path-consumers"
    prepare(root, out)
    cc = ["clang", "-std=gnu11", "-O1", "-g", "-Wall", "-Wextra", "-Werror", "-Wno-unused-function",
          "-Wno-unused-parameter", "-Wno-sign-compare", "-fsanitize=address,undefined", "-fno-sanitize-recover=undefined",
          "-fno-omit-frame-pointer", "-pthread", "-I", str(out)]
    fixture = root / "scripts/susfs/test_path_consumers.c"
    results = {"scope": __doc__, "compiler": run(["clang", "--version"]).splitlines()[0], "runs": []}
    for path in [False, True]:
        for compat in [False, True]:
            name = f"path-{int(path)}-compat-{int(compat)}"
            flags = ["-DCONFIG_ARM64=1", "-DCONFIG_64BIT=1", "-DCONFIG_THREAD_INFO_IN_TASK=1"]
            if path: flags += ["-DCONFIG_KSU_SUSFS_SUS_PATH=1", "-DCONFIG_KSU_SUSFS=1", "-DCONFIG_KSU=1", "-DCONFIG_KSU_DEVON_SELINUX_POLICY=1"]
            if compat: flags += ["-DCONFIG_COMPAT=1"]
            (out / (name+"-compile.log")).write_text(run(cc + flags + [str(fixture), "-o", str(out/name)]))
            result = run([str(out/name)])
            (out / (name+".log")).write_text(result)
            results["runs"].append({"name": name, "exit": 0, "result": result.strip()})
            print(name+": "+result.strip())
    # Actual-source mutation controls must fail the observable open/EOF assertions.
    path_flags = ["-DCONFIG_ARM64=1", "-DCONFIG_64BIT=1", "-DCONFIG_THREAD_INFO_IN_TASK=1",
                  "-DCONFIG_KSU_SUSFS_SUS_PATH=1", "-DCONFIG_COMPAT=1", "-DCONFIG_KSU_SUSFS=1", "-DCONFIG_KSU=1", "-DCONFIG_KSU_DEVON_SELINUX_POLICY=1"]
    originals = {f: (out/f).read_text() for f in ["namei_under_test.h", "readdir_under_test.h"]}
    mutations = [
        ("reject-atomic-open-before-filter", "namei_under_test.h",
         "if (susfs_is_current_proc_umounted_app())", "if (false)"),
        ("reject-post-create-inode-exposure", "namei_under_test.h",
         'out_no_open:\n#ifdef CONFIG_KSU_SUSFS_SUS_PATH\n\t/* A successful ->create() may instantiate a different, marked inode. */\n\tif (susfs_is_inode_sus_path(d_backing_inode(dentry))) {\n\t\terror = -ENOENT;\n\t\tgoto out_dput;\n\t}\n#endif\n',
         'out_no_open:\n'),
        ("reject-hidden-entry-buffer-error", "readdir_under_test.h",
         "if (susfs_dirent_hidden(buf->dir, name, namlen, ino))\n\t\treturn 0;",
         "if (susfs_dirent_hidden(buf->dir, name, namlen, ino))\n\t\treturn -EINVAL;"),
        ("reject-overlay-inode-number-guess", "readdir_under_test.h",
         'if (dir->d_sb->s_magic == OVERLAYFS_SUPER_MAGIC)\n\t\treturn false;',
         'if (false)\n\t\treturn false;'),
    ]
    try:
        for name, file, old, new in mutations:
            assert old in originals[file]
            (out/file).write_text(originals[file].replace(old, new))
            binary = out/name
            (out/(name+"-compile.log")).write_text(run(cc+path_flags+[str(fixture), "-o", str(binary)]))
            rejected = subprocess.run([str(binary)], capture_output=True, text=True)
            (out/(name+".log")).write_text(rejected.stdout+rejected.stderr)
            assert rejected.returncode and "Assertion" in rejected.stderr, name
            (out/file).write_text(originals[file])
    finally:
        for file, original in originals.items(): (out/file).write_text(original)
    results["mutation_controls"] = "Atomic-open filter bypass, post-create inode exposure, hidden-entry buffer errors and overlay inode-number guessing fail assertions"
    results["overlay_enumeration"] = "Generic emitters check named cached overlay children and never guess overlay inode numbers. Separate overlay tests cover merged and direct iteration with deferred native view lookup."
    results["open_scope"] = "Actual native atomic_open and lookup_open run with modeled filesystem callbacks; post-create inode/owner/marker transitions, errors and native create notification are checked. No native remote server or concurrent VFS is run."
    # A cached positive name must not become a cache miss shared with other tasks.
    patched = (out / "namei_under_test.h").read_text()
    assert "susfs_fake_qstr_name" not in patched and "d_lookup_done" not in function((root/"fs/namei.c").read_text(), "lookup_fast")
    results["disabled_source_identity"] = "Removing only inserted guards exactly restores both native files"
    results["production_gate"] = "Conditional SUS_PATH consumers and capability output are implemented; native runtime remains unvalidated"
    files = [Path(__file__).resolve(), fixture, root/"fs/namei.c", root/"fs/readdir.c", root/"fs/dcache.c", root/"fs/susfs_path.c",
             root/"fs/susfs.c", root/"include/linux/susfs.h", root/"include/linux/susfs_def.h", root/"include/linux/fs.h",
             root/"include/linux/dcache.h", root/"include/linux/mount.h", root/"scripts/susfs/source-pins.json",
             root/"fs/susfs_fuse.c", root/"include/linux/susfs_fuse.h", root/"include/linux/list.h", root/"include/linux/hashtable.h",
             root/"scripts/susfs/fuse_index_model.h", root/"include/linux/ksu_susfs.h", root/"scripts/susfs/test_selinux_adapter.py", root/"scripts/susfs/test_setresuid_task_state.py", root/"scripts/susfs/source_guards.py"]
    results["inputs"] = {str(f.relative_to(root)): digest(f) for f in files}
    (out / "results.json").write_text(json.dumps(results, indent=2)+"\n")
    print("Source/host consumer checks passed; native VFS/FUSE and aggregate enablement remain unvalidated.")


if __name__ == "__main__":
    main()
