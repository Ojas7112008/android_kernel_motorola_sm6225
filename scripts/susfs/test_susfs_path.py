#!/usr/bin/env python3
"""Isolated SUS_PATH core object and real command/work consumers under host models.

The production aggregate still rejects SUS_PATH until lookup/readdir consumers
are ported. This test does not relax that guard or build a kernel object/image.
"""
import json
from pathlib import Path
import re
import subprocess
import sys
sys.dont_write_bytecode = True
from test_selinux_adapter import function, run, digest
from test_selinux_autohooks import PIN
from test_setresuid_task_state import section

SUSFS_PIN = "82f08351fadb8b19dd6591bd14634d02ad832145"


def main():
    root = Path(__file__).resolve().parents[2]
    ref = root / ".backport-context/repos/bakasu-sync-susfs"
    susfs = root / ".backport-context/repos/susfs"
    for repo, pin in [(ref, PIN), (susfs, SUSFS_PIN)]:
        assert run(["git", "-C", str(repo), "rev-parse", "HEAD"]).strip() == pin
        assert not run(["git", "-C", str(repo), "status", "--porcelain"])
    out = root / ".backport-context/audit/path-core"
    out.mkdir(parents=True, exist_ok=True)
    inc = out / "linux"
    inc.mkdir(exist_ok=True)
    for name in ["susfs.h", "susfs_def.h", "ksu_susfs.h"]:
        (inc / name).write_bytes((root / "include/linux" / name).read_bytes())
    (inc / "types.h").write_text("#pragma once\n#include_next <linux/types.h>\n#include <stdint.h>\n#include <sys/types.h>\ntypedef uint32_t u32; typedef uint16_t u16; typedef uint64_t u64;\n")
    (inc / "errno.h").write_text("#include_next <linux/errno.h>\n")
    (inc / "export.h").write_text("#define EXPORT_SYMBOL_GPL(name) static typeof(name) *exported_##name __attribute__((unused)) = &name\n")
    (inc / "bits.h").write_text("#define BIT(n) (1UL<<(n))\n")
    (inc / "version.h").write_text("#define KERNEL_VERSION(a,b,c) (((a)<<16)+((b)<<8)+(c))\n#define LINUX_VERSION_CODE KERNEL_VERSION(4,19,255)\n")
    for name in ["cred.h", "fs.h", "fs_struct.h", "init_task.h", "mutex.h", "namei.h", "nsproxy.h", "printk.h", "rculist.h",
                 "slab.h", "srcu.h", "string.h", "uaccess.h", "workqueue.h", "sched.h", "thread_info.h", "uidgid.h",
                 "utsname.h", "hashtable.h", "path.h", "statfs.h"]:
        (inc / name).write_text("/* Model declarations precede real path source. */\n")
    (inc / "overflow.h").write_text("/* Actual checked helper/macro is in the fixture prefix. */\n")
    overflow = (root / "include/linux/overflow.h").read_text()
    native_overflow = section(overflow, "static inline bool __must_check __must_check_overflow(")
    native_overflow += re.search(r"^#define check_add_overflow\(a, b, d\)[^\n]*\n[^\n]*__builtin_add_overflow[^\n]+", overflow, re.M).group(0) + "\n"
    (out / "native_overflow.h").write_text(native_overflow)
    (out / "path_object.h").write_bytes((root / "fs/susfs_path.c").read_bytes())
    core = (root / "fs/susfs.c").read_text()
    names = ["susfs_reply_error", "susfs_get_enabled_features", "susfs_show_variant", "susfs_show_version", "susfs_set_avc_log_spoofing", "susfs_run_extra_works"]
    code = "".join(function(core, name) for name in names)
    code += re.search(r"^DECLARE_WORK\(susfs_extra_works, susfs_run_extra_works\);", core, re.M).group(0) + "\n"
    (out / "core_command_work.h").write_text(code)
    (out / "pinned_dispatch.h").write_text(function((ref / "kernel/supercall/dispatch.c").read_text(), "ksu_handle_susfs_cmd"))
    (out / "pinned_transport.h").write_text(function((ref / "kernel/supercall/supercall.c").read_text(), "ksu_handle_sys_reboot"))
    magic = (ref / "uapi/supercall.h").read_text()
    constants = re.findall(r"DEFINE_KSU_UAPI_CONST\(__u32, (KSU_INSTALL_MAGIC[12]), (0x[0-9A-F]+)\)", magic)
    assert len(constants) == 2
    (out / "reboot_magic.h").write_text("".join(f"#define {name} {value}U\n" for name, value in constants))
    rcu = (root / "include/linux/rculist.h").read_text()
    (out / "native_rculist.h").write_text(re.search(r"^#define list_next_rcu[^\n]+", rcu, re.M).group(0) + "\n" +
        function((root / "include/linux/list.h").read_text(), "INIT_LIST_HEAD") + function(rcu, "__list_add_rcu") + function(rcu, "list_add_tail_rcu"))
    srcu = (root / "include/linux/srcu.h").read_text()
    check = re.search(r"^#define srcu_dereference_check\(p, sp, c\) \\\n[^\n]+", srcu, re.M).group(0)
    dereference = re.search(r"^#define srcu_dereference\(p, sp\)[^\n]+", srcu, re.M).group(0)
    (out / "native_srcu_macros.h").write_text(check + "\n" + dereference + "\n")
    (out / "native_root_helpers.h").write_text(function((root / "include/linux/fs_struct.h").read_text(), "get_fs_root") +
        function((root / "include/linux/path.h").read_text(), "path_equal"))
    (out / "native_fuse_identity.h").write_text(function((root / "fs/fuse/fuse_i.h").read_text(), "get_fuse_inode"))
    upstream = (susfs / "kernel_patches/fs/susfs.c").read_text()
    negative = section(upstream, "void susfs_add_sus_path_loop(").replace("void susfs_add_sus_path_loop(", "void pinned_add_sus_path_loop(", 1)
    (out / "pinned_path_negative.h").write_text(negative)
    # Native low mapping flags remain below the upstream SUS_PATH bit 33.
    native_flags = (root / "include/linux/pagemap.h").read_text()
    values = re.findall(r"\b(AS_\w+)\s*=\s*(\d+)", native_flags)
    assert values and all(int(value) < 33 for _, value in values)
    assert "feature consumers are not ported" not in core
    assert "CONFIG_KSU_SUSFS_SUS_PATH\\n" in function(core, "susfs_get_enabled_features")
    assert "obj-$(CONFIG_KSU_SUSFS_SUS_PATH)\t+= susfs_path.o susfs_fuse.o" in (root / "fs/Makefile").read_text()
    fixture = root / "scripts/susfs/test_susfs_path.c"
    cc = ["clang", "-std=gnu11", "-O1", "-g", "-pthread", "-D_GNU_SOURCE", "-Wall", "-Wextra", "-Werror",
          "-Wno-unused-parameter", "-Wno-unused-function", "-Wno-unused-variable", "-Wno-sign-compare",
          "-Werror=incompatible-pointer-types", "-fsanitize=address,undefined", "-fno-sanitize-recover=undefined", "-fno-omit-frame-pointer", "-I", str(out)]
    enabled = ["-DCONFIG_KSU_SUSFS=1", "-DCONFIG_KSU=1", "-DCONFIG_KSU_DEVON_SELINUX_POLICY=1",
               "-DCONFIG_ARM64=1", "-DCONFIG_64BIT=1", "-DCONFIG_THREAD_INFO_IN_TASK=1",
               "-DCONFIG_KSU_SUSFS_SUS_PATH=1", "-DCONFIG_SRCU=1"]
    results = {"pins": {"bakasu": PIN, "susfs": SUSFS_PIN}, "scope": __doc__, "compiler": run(["clang", "--version"]).splitlines()[0], "runs": []}
    for key in [False, True]:
        name = "path-static-key-" + str(int(key))
        flags = enabled + (["-DKSU_COMPAT_USE_STATIC_KEY=1"] if key else [])
        binary = out / name
        (out / (name + "-compile.log")).write_text(run(cc + flags + [str(fixture), "-o", str(binary)]))
        result = run([str(binary)])
        (out / (name + ".log")).write_text(result)
        print(name + ": " + result.strip())
        results["runs"].append({"name": name, "exit": 0, "result": result.strip()})
    no_srcu = subprocess.run(cc + [arg for arg in enabled if not arg.startswith("-DCONFIG_SRCU")] + ["-fsyntax-only", str(fixture)], capture_output=True, text=True)
    (out / "reject-no-srcu.log").write_text(no_srcu.stderr)
    assert no_srcu.returncode and "requires SUS_PATH and SRCU" in no_srcu.stderr
    prefix = fixture.read_text().split('#include "path_object.h"')[0] + "\n#include <linux/susfs.h>\n"
    for name, declaration in [("reject-legacy-path-payload-pointer", "void susfs_add_sus_path(struct st_susfs_sus_path *);"),
                              ("reject-untyped-inode-predicate", "bool susfs_is_inode_sus_path(void *);")]:
        rejected = subprocess.run(cc + enabled + ["-x", "c", "-fsyntax-only", "-"], input=prefix+declaration+"\n", capture_output=True, text=True)
        (out / (name+".log")).write_text(rejected.stderr)
        assert rejected.returncode and "conflicting types" in rejected.stderr
    results["prototype_rejections"] = "legacy payload pointer and untyped inode predicate fail compilation"
    results["production_gate"] = "SUS_PATH has conditional objects and capability output; native build/runtime validation remains separate"
    results["abi"] = validate_abi(root, out, susfs, ref)
    inputs = [Path(__file__).resolve(), fixture, root / "scripts/susfs/test_selinux_adapter.py", root / "scripts/susfs/test_selinux_autohooks.py",
              root / "scripts/susfs/test_setresuid_task_state.py", root / "scripts/susfs/source-pins.json", root / "fs/susfs_path.c", root / "fs/susfs.c",
              root / "fs/Makefile", root / "include/linux/susfs.h", root / "include/linux/overflow.h", root / "include/linux/susfs_def.h", root / "include/linux/ksu_susfs.h",
              root / "include/linux/fs_struct.h", root / "include/linux/path.h", root / "include/linux/nsproxy.h", root / "include/linux/rculist.h",
              root / "include/linux/srcu.h", root / "include/linux/list.h", root / "include/linux/pagemap.h", root / "fs/fuse/fuse_i.h", ref / "kernel/supercall/supercall.c",
              ref / "kernel/supercall/dispatch.c", ref / "uapi/supercall.h", susfs / "kernel_patches/fs/susfs.c", susfs / "kernel_patches/include/linux/susfs.h",
              susfs / "ksu_susfs/jni/features/sus_path.c", ref / "userspace/ksud/src/android/susfs/api/features/sus_path.rs"]
    results["inputs"] = {str(p.relative_to(root)): digest(p) for p in inputs}
    (out / "results.json").write_text(json.dumps(results, indent=2) + "\n")
    print("Path-core host checks pass; feature consumers/aggregate kernel/native SRCU and namespace behavior remain unvalidated.")


def validate_abi(root, out, susfs, ref):
    inc = out / "abi/linux"
    inc.mkdir(parents=True, exist_ok=True)
    for name in ["susfs.h", "susfs_def.h"]:
        (inc / name).write_bytes((root / "include/linux" / name).read_bytes())
    (inc / "types.h").write_text("#pragma once\n#include <stdbool.h>\n#include <stddef.h>\ntypedef unsigned int uid_t;\n#define __user\n")
    (inc / "version.h").write_bytes((out / "linux/version.h").read_bytes())
    (inc / "string.h").write_text("size_t strlen(const char *); int strcmp(const char *,const char *);\n")
    (inc / "sched.h").write_text("struct uid_model { uid_t val; }; struct uid_model current_uid(void);\n")
    (inc / "thread_info.h").write_text("#define likely(v) (v)\nint test_thread_flag(int); void set_thread_flag(int); void clear_thread_flag(int);\n")
    (inc / "hashtable.h").write_text("struct list_head { struct list_head *next, *prev; };\n")
    (inc / "path.h").write_text("struct path;\n")
    for name in ["cred.h", "bits.h", "uidgid.h", "utsname.h", "statfs.h"]:
        (inc / name).write_text("/* Other feature payloads are disabled. */\n")
    c = (susfs / "ksu_susfs/jni/features/sus_path.c").read_text()
    user = re.search(r"struct st_susfs_sus_path \{.*?\n\};", c, re.S).group(0).replace("struct st_susfs_sus_path", "struct jni_sus_path")
    code = "#include <stdbool.h>\n#include <stddef.h>\n#include <linux/susfs.h>\n" + user
    code += '\n_Static_assert(sizeof(struct st_susfs_sus_path)==260 && offsetof(struct st_susfs_sus_path,err)==256, "path ABI");\n'
    code += '_Static_assert(sizeof(struct st_susfs_sus_path)==sizeof(struct jni_sus_path) && offsetof(struct st_susfs_sus_path,err)==offsetof(struct jni_sus_path,err), "JNI ABI");\n'
    code += '_Static_assert(CMD_SUSFS_ADD_SUS_PATH==0x55550 && CMD_SUSFS_ADD_SUS_PATH_LOOP==0x55553, "commands");\n'
    (out / "path-payload-abi.c").write_text(code)
    rust = (ref / "userspace/ksud/src/android/susfs/api/features/sus_path.rs").read_text()
    assert "#[repr(C)]" in rust and "target_pathname: [u8; SUSFS_MAX_LEN_PATHNAME]" in rust and "err: i32" in rust
    runs = []
    for target in ["aarch64-linux-android", "armv7a-linux-androideabi"]:
        cmd = ["clang", "--target=" + target, "-std=gnu11", "-ffreestanding", "-fsyntax-only", "-Werror", "-DCONFIG_KSU_SUSFS_SUS_PATH=1",
               "-I", str(out / "abi"), str(out / "path-payload-abi.c")]
        (out / (target + "-path-abi.log")).write_text(run(cmd))
        runs.append({"target": target, "exit": 0, "bytes": 260, "err_offset": 256})
    return {"runs": runs, "meaning": "path payload matches actual JNI and inspected Rust repr(C); not a kernel object/compat transport test"}


if __name__ == "__main__":
    main()
