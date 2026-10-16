#!/usr/bin/env python3
"""Whole staged SUSFS core under host models plus real header target ABI probes.

Does not compile a kernel object, run actual fsnotify/VFS/workqueues, or boot devon.
"""
import json
from pathlib import Path
import re
from kernel_baseline import kernel_source
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
    out = root / ".backport-context/audit/core-foundation"
    out.mkdir(parents=True, exist_ok=True)
    inc = out / "linux"
    inc.mkdir(exist_ok=True)
    header = root / "include/linux/susfs.h"
    declaration_delta = '/* Devon 4.19 path-core providers; consumers remain a separate gated stage. */\n#ifdef CONFIG_KSU_SUSFS_SUS_PATH\nstruct inode;\nstruct qstr;\nstruct work_struct;\nextern struct work_struct susfs_extra_works;\nbool susfs_is_inode_sus_path(struct inode *inode);\nint susfs_get_data_path(struct path *path);\nvoid susfs_run_sus_path_loop(void);\nextern const struct qstr susfs_fake_qstr_name;\n#endif\n\n'
    assert header.read_text().replace(declaration_delta, "", 1) == (susfs / "kernel_patches/include/linux/susfs.h").read_text()
    for name in ["susfs.h", "susfs_def.h", "ksu_susfs.h"]:
        (inc / name).write_bytes((root / "include/linux" / name).read_bytes())
    (inc / "types.h").write_text("#pragma once\n#include_next <linux/types.h>\n#include <stdint.h>\n#include <sys/types.h>\ntypedef uint32_t u32; typedef uint16_t u16; typedef uint64_t u64;\n")
    (inc / "version.h").write_text("#define KERNEL_VERSION(a,b,c) (((a)<<16)+((b)<<8)+(c))\n#define LINUX_VERSION_CODE KERNEL_VERSION(4,19,255)\n")
    (inc / "bits.h").write_text("#define BIT(n) (1UL<<(n))\n")
    for name in ["atomic.h", "cred.h", "err.h", "errno.h", "fs.h", "fsnotify_backend.h", "jiffies.h", "jump_label.h", "kernel.h", "mutex.h",
                 "namei.h", "slab.h", "string.h", "uaccess.h", "workqueue.h", "sched.h", "thread_info.h", "uidgid.h",
                 "utsname.h", "hashtable.h", "path.h", "statfs.h"]:
        (inc / name).write_text("/* Model declarations precede whole core source. */\n")
    (inc / "errno.h").write_text("#include_next <linux/errno.h>\n")
    (inc / "overflow.h").write_text("/* Actual checked helper/macro is in the fixture prefix. */\n")
    overflow = (root / "include/linux/overflow.h").read_text()
    native_overflow = section(overflow, "static inline bool __must_check __must_check_overflow(")
    native_overflow += re.search(r"^#define check_add_overflow\(a, b, d\)[^\n]*\n[^\n]*__builtin_add_overflow[^\n]+", overflow, re.M).group(0) + "\n"
    (out / "native_overflow.h").write_text(native_overflow)
    fsnotify = (root / "include/linux/fsnotify_backend.h").read_text()
    native_ops = re.search(r"struct fsnotify_ops \{.*?\n\};", fsnotify, re.S).group(0)
    constants = re.findall(r"^#define FS_(?:EVENT_ON_CHILD|ISDIR|OPEN|OPEN_PERM|CREATE|MOVED_TO|DELETE_SELF|MOVE_SELF)\s+[^\n]+", fsnotify, re.M)
    (out / "native_fsnotify_ops.h").write_text("\n".join(constants) + "\n" + native_ops + "\n")
    marks = (root / "fs/notify/mark.c").read_text()
    groups = (root / "fs/notify/group.c").read_text()
    (out / "native_mark_refs.h").write_text(function(groups, "fsnotify_get_group") + function(groups, "fsnotify_put_group") +
        function(marks, "fsnotify_final_mark_destroy") + function(marks, "fsnotify_init_mark"))
    (out / "native_destroy_group.h").write_text(function(groups, "fsnotify_destroy_group"))
    core = (root / "fs/susfs.c").read_text()
    (out / "core_under_test.h").write_text(core)
    dispatch = (ref / "kernel/supercall/dispatch.c").read_text()
    (out / "pinned_dispatch.h").write_text(function(dispatch, "ksu_handle_susfs_cmd"))
    transport = (ref / "kernel/supercall/supercall.c").read_text()
    (out / "pinned_transport.h").write_text(function(transport, "ksu_handle_sys_reboot"))
    magic = (ref / "uapi/supercall.h").read_text()
    constants = re.findall(r"DEFINE_KSU_UAPI_CONST\(__u32, (KSU_INSTALL_MAGIC[12]), (0x[0-9A-F]+)\)", magic)
    assert len(constants) == 2
    (out / "reboot_magic.h").write_text("".join(f"#define {name} {value}U\n" for name, value in constants))
    upstream = (susfs / "kernel_patches/fs/susfs.c").read_text()
    (out / "pinned_features_negative.h").write_text(section(upstream, "static int copy_config_to_buf(") +
        section(upstream, "void susfs_get_enabled_features(").replace("void susfs_get_enabled_features(", "void pinned_get_enabled_features(", 1))
    watch = re.search(r"struct watch_dir \{.*?\n\};", upstream, re.S).group(0)
    watch += "\nstatic struct fsnotify_group *g;\nstatic struct watch_dir g_watch;\n"
    watch += function(upstream, "add_mark_on_inode") + function(upstream, "watch_one_dir") + function(upstream, "susfs_sdcard_cleanup_fn")
    (out / "pinned_watch_negative.h").write_text(watch)
    cc = ["clang", "-std=gnu11", "-O1", "-g", "-pthread", "-D_GNU_SOURCE", "-Wall", "-Wextra", "-Werror",
          "-Wno-unused-parameter", "-Wno-unused-function", "-Wno-unused-variable", "-Wno-sign-compare",
          "-Werror=incompatible-pointer-types", "-fsanitize=address,undefined", "-fno-sanitize-recover=undefined", "-fno-omit-frame-pointer", "-I", str(out)]
    enabled = ["-DCONFIG_KSU_SUSFS=1", "-DCONFIG_KSU=1", "-DCONFIG_KSU_DEVON_SELINUX_POLICY=1",
               "-DCONFIG_ARM64=1", "-DCONFIG_64BIT=1", "-DCONFIG_THREAD_INFO_IN_TASK=1", "-DCONFIG_FSNOTIFY=1"]
    fixture = root / "scripts/susfs/test_susfs_core.c"
    results = {"pins": {"bakasu": PIN, "susfs": SUSFS_PIN}, "scope": __doc__, "compiler": run(["clang", "--version"]).splitlines()[0], "runs": []}
    for log in [False, True]:
        for key in [False, True]:
            name = f"core-log-{int(log)}-compat-static-key-{int(key)}"
            flags = enabled + (["-DCONFIG_KSU_SUSFS_ENABLE_LOG=1"] if log else []) + (["-DKSU_COMPAT_USE_STATIC_KEY=1"] if key else [])
            binary = out / name
            compiled = run(cc + flags + [str(fixture), "-o", str(binary)])
            (out / (name + "-compile.log")).write_text(compiled)
            result = run([str(binary)])
            (out / (name + ".log")).write_text(result)
            results["runs"].append({"name": name, "exit": 0, "result": result.strip()})
            print(name + ": " + result.strip())
    no_fsnotify = subprocess.run(cc + [f for f in enabled if not f.startswith("-DCONFIG_FSNOTIFY")] + ["-fsyntax-only", str(fixture)], capture_output=True, text=True)
    (out / "reject-no-fsnotify.log").write_text(no_fsnotify.stderr)
    assert no_fsnotify.returncode and "requires its checked built-in" in no_fsnotify.stderr
    # Original callback/ops must fail against the real 4.19 function table.
    probe = "#include <linux/types.h>\nstruct inode; struct fsnotify_mark; struct fsnotify_group; struct fsnotify_event; struct fsnotify_iter_info; struct qstr;\n" + native_ops
    probe += "\nstatic int old(struct fsnotify_mark *m, u32 mask, struct inode *i, struct inode *d, const struct qstr *n, u32 c);\n"
    probe += "static const struct fsnotify_ops ops = { .handle_inode_event = old };\n"
    bad = subprocess.run(cc + ["-x", "c", "-fsyntax-only", "-"], input=probe, capture_output=True, text=True)
    (out / "pinned-fsnotify-ops-negative.log").write_text(bad.stderr)
    assert bad.returncode and "handle_inode_event" in bad.stderr
    results["rejections"] = "absent FSNOTIFY and original modern ops fail compilation; feature-specific consumers have separate suites"
    results["abi"] = validate_abi(root, out, susfs, ref)
    # Preserve unchanged external provider; startup consumers already call these entry points.
    init = root / json.loads((root / 'scripts/susfs/build-pins.json').read_text())['dependency_directory'] / 'kernel/core/init.c'
    assert "susfs_init();" in section(init.read_text(), "static inline void __init ksu_hook_init(")
    event = function(dispatch, "do_report_event")
    assert "on_boot_completed();" in event and "susfs_start_sdcard_monitor_fn();" in event
    assert "obj-$(CONFIG_KSU_SUSFS)\t\t+= susfs.o" in (root / "fs/Makefile").read_text()
    inputs = [Path(__file__).resolve(), root / "scripts/susfs/source-pins.json", root / "scripts/susfs/test_selinux_adapter.py",
              root / "scripts/susfs/test_selinux_autohooks.py", root / "scripts/susfs/test_setresuid_task_state.py", fixture, root / "fs/susfs.c", root / "fs/Makefile", header,
              root / "include/linux/overflow.h", root / "include/linux/susfs_def.h", root / "include/linux/ksu_susfs.h", root / "include/linux/fsnotify_backend.h",
              root / "fs/notify/mark.c", root / "fs/notify/group.c", ref / "kernel/supercall/dispatch.c",
              ref / "kernel/supercall/supercall.c", ref / "uapi/supercall.h", susfs / "kernel_patches/fs/susfs.c",
              susfs / "kernel_patches/include/linux/susfs.h", init, root / ".github/patches/bakasu-devon-selinux-policy.patch"]
    inputs += [susfs / "ksu_susfs/jni/features" / name for name in ["show.c", "enable_log.c", "avc_log_spoofing.c"]]
    inputs += [ref / "userspace/ksud/src/android/susfs/api/features" / name for name in ["show.rs", "enable_log.rs", "avc_spoofing.rs"]]
    results["inputs"] = {str(p.relative_to(root)): digest(p) for p in inputs}
    (out / "results.json").write_text(json.dumps(results, indent=2) + "\n")
    print("Core host checks and supported payload target layouts pass; full kernel and native runtime behavior remain unvalidated.")


def validate_abi(root, out, susfs, ref):
    # Freestanding target syntax probes use the actual kernel header and exact JNI structs.
    inc = out / "abi/linux"
    inc.mkdir(parents=True, exist_ok=True)
    for name in ["susfs.h", "susfs_def.h"]:
        (inc / name).write_bytes((root / "include/linux" / name).read_bytes())
    (inc / "types.h").write_text("#pragma once\n#include <stdbool.h>\n#include <stddef.h>\ntypedef unsigned int u32; typedef unsigned int uid_t;\n#define __user\n")
    (inc / "version.h").write_bytes((out / "linux/version.h").read_bytes())
    (inc / "string.h").write_text("size_t strlen(const char *); int strcmp(const char *,const char *);\n")
    (inc / "sched.h").write_text("struct uid_model { uid_t val; }; struct uid_model current_uid(void);\n")
    (inc / "thread_info.h").write_text("#define likely(v) (v)\nint test_thread_flag(int); void set_thread_flag(int); void clear_thread_flag(int);\n")
    for name in ["cred.h", "bits.h", "uidgid.h", "utsname.h", "hashtable.h", "path.h", "statfs.h"]:
        (inc / name).write_text("/* No internal gated structures needed for these payloads. */\n")
    names = ["st_susfs_enabled_features", "st_susfs_version", "st_susfs_variant", "st_susfs_log", "st_susfs_avc_log_spoofing"]
    source = "#include <stdbool.h>\n#include <stddef.h>\n#include <linux/susfs.h>\n"
    text = "\n".join((susfs / "ksu_susfs/jni/features" / f).read_text() for f in ["show.c", "enable_log.c", "avc_log_spoofing.c"])
    for name in names:
        body = re.search(r"struct " + name + r" \{.*?\n\};", text, re.S).group(0)
        user_name = "jni_" + name
        source += body.replace("struct " + name, "struct " + user_name) + "\n"
        size, offset = (8196, 8192) if "enabled_features" in name else ((8, 4) if name in ["st_susfs_log", "st_susfs_avc_log_spoofing"] else (20, 16))
        for assertion in [f"sizeof(struct {name}) == {size}", f"offsetof(struct {name}, err) == {offset}",
                          f"sizeof(struct {name}) == sizeof(struct {user_name})", f"offsetof(struct {name}, err) == offsetof(struct {user_name}, err)"]:
            source += '_Static_assert(' + assertion + ', "payload ABI");\n'
    source += '_Static_assert(CMD_SUSFS_SHOW_VERSION == 0x555e1 && CMD_SUSFS_SHOW_VARIANT == 0x555e3 && CMD_SUSFS_SHOW_ENABLED_FEATURES == 0x555e2, "commands");\n'
    (out / "payload-abi.c").write_text(source)
    for name in ["show.rs", "enable_log.rs", "avc_spoofing.rs"]:
        rust = (ref / "userspace/ksud/src/android/susfs/api/features" / name).read_text()
        assert "#[repr(C)]" in rust and "err: i32" in rust
        if name != "show.rs": assert "enabled: bool" in rust
    runs = []
    for target in ["aarch64-linux-android", "armv7a-linux-androideabi"]:
        cmd = ["clang", "--target=" + target, "-std=gnu11", "-ffreestanding", "-fsyntax-only", "-Werror",
               "-DCONFIG_KSU_SUSFS_ENABLE_LOG=1", "-I", str(out / "abi"), str(out / "payload-abi.c")]
        (out / (target + "-payload-abi.log")).write_text(run(cmd))
        runs.append({"target": target, "exit": 0, "payloads": len(names)})
    return {"runs": runs, "meaning": "five fixed-width supported core payloads match JNI sizeof/err offsets; BakaSU repr(C) inspected; not a kernel object build or full feature/compat ABI"}


if __name__ == "__main__":
    main()
