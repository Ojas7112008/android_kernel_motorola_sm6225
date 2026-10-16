#!/usr/bin/env python3
"""Actual access/stat and filename source under host mocks; no ARM64/kernel runtime proof."""
import json
from pathlib import Path
import re
from kernel_baseline import kernel_source
import subprocess
import sys
import tempfile
sys.dont_write_bytecode = True
from test_selinux_autohooks import PATCH_FILES, PIN, MACRO
from test_selinux_adapter import run, digest, function
from test_setresuid_task_state import section
from source_guards import without_feature_guards

def main():
    root = Path(__file__).resolve().parents[2]
    ref = root / ".backport-context/repos/bakasu-sync-susfs"
    assert run(["git", "-C", str(ref), "rev-parse", "HEAD"]).strip() == PIN
    assert not run(["git", "-C", str(ref), "status", "--porcelain"])
    output = root / ".backport-context/audit/access-stat"
    output.mkdir(parents=True, exist_ok=True)
    patch = root / ".github/patches/bakasu-devon-selinux-policy.patch"
    fixture = root / "scripts/susfs/test_access_stat.c"
    results = {"pin": PIN, "scope": __doc__, "compiler": run(["clang", "--version"]).splitlines()[0], "runs": []}
    with tempfile.TemporaryDirectory(prefix="devon-access-stat-") as directory:
        work = Path(directory)
        for name in PATCH_FILES:
            p = work / name
            p.parent.mkdir(parents=True, exist_ok=True)
            p.write_text(run(["git", "-C", str(ref), "show", PIN + ":" + name]))
        run(["git", "init", "--quiet"], cwd=work)
        run(["git", "add", "--", *PATCH_FILES], cwd=work)
        run(["git", "apply", "--check", "--whitespace=error-all", str(patch)], cwd=work)
        run(["git", "apply", "--whitespace=error-all", str(patch)], cwd=work)
        run(["git", "diff", "--check"], cwd=work)
        run(["git", "apply", "--reverse", "--check", str(patch)], cwd=work)
        assert sorted(run(["git", "diff", "--name-only"], cwd=work).splitlines()) == sorted(PATCH_FILES)
        names = (root / "fs/namei.c").read_text()
        native_names = "\n".join(function(names, name) for name in ["getname_flags", "getname_kernel", "putname"])
        # Both functions are non-static in the real source.
        native_names = native_names.replace("void putname(", "static void putname(")
        (output / "native_names.h").write_text(native_names)
        (output / "native_lookup.h").write_text(function(names, "filename_lookup"))
        stat = (root / "fs/stat.c").read_text()
        (output / "native_getattr.h").write_text(function(stat, "vfs_getattr_mnt_id") + function(stat, "vfs_getattr"))
        access = (root / "fs/open.c").read_text()
        (output / "kernel_access_stat.h").write_text(section(access, "long do_faccessat(") +
            function(stat, "vfs_statx_mnt_id") + function(stat, "vfs_statx"))
        fs_header = (root / "include/linux/fs.h").read_text()
        (output / "native_stat_wrappers.h").write_text("".join(function(fs_header, name) for name in
            ["vfs_stat", "vfs_lstat", "vfs_fstatat"]))
        # Common helpers remain reachable from access/faccessat and native/compat
        # pathname-stat wrappers; manual entry hooks are unchanged.
        baseline = "18bd9752ada79090ce11326662eaaa9f33717728"
        for source, name, marker in [(access,"fs/open.c","SYSCALL_DEFINE3(faccessat,"),
                                     (access,"fs/open.c","SYSCALL_DEFINE2(access,"),
                                     (stat,"fs/stat.c","SYSCALL_DEFINE4(newfstatat,")]:
            original = kernel_source(root, name, legacy=True)
            assert section(source, marker) == section(original, marker)
        for source, name, common in [(access, "fs/open.c", "long do_faccessat("), (stat, "fs/stat.c", "int vfs_statx(")]:
            original = kernel_source(root, name, legacy=True)
            restored = source
            if name == "fs/stat.c":
                # Private statx transport is checked by its dedicated suite.
                # Everything outside these reviewed functions retains native code.
                for public in ["vfs_getattr_nosec", "vfs_getattr", "vfs_statx"]:
                    restored = restored.replace(function(source, public), "", 1)
                    restored = restored.replace(function(source, public + "_mnt_id"),
                                                function(original, public), 1)
                for marker in ["static noinline_for_stack int\ncp_statx(", "SYSCALL_DEFINE5(statx,"]:
                    restored = restored.replace(section(source, marker), section(original, marker), 1)
                restored = restored.replace(function(source, "vfs_statx_fd"), function(original, "vfs_statx_fd"), 1)
            else:
                restored = restored.replace(section(source, common), section(original, common), 1)
            private_include = '#include "internal.h"\n' if name == "fs/stat.c" else ""
            restored = restored.replace("#ifdef CONFIG_KSU_SUSFS\n#include <linux/ksu_susfs.h>\n" + private_include + "#endif\n", "", 1)
            restored = without_feature_guards(restored, ["CONFIG_KSU_SUSFS_SUS_MOUNT", "CONFIG_KSU_SUSFS_SUS_KSTAT"])
            def tokens(text):
                return re.sub(r"\s+", " ", re.sub(r"/\*.*?\*/|//[^\n]*", "", text, flags=re.S)).strip()
            assert tokens(restored) == tokens(original), "Unexpected change outside reviewed stat/lookup functions: " + name
        include = output / "linux"
        include.mkdir(exist_ok=True)
        (include / "types.h").write_text("#ifndef ACCESS_TEST_TYPES\n#define ACCESS_TEST_TYPES\n#include <stdint.h>\n#include <sys/types.h>\n#ifndef __user\n#define __user\n#endif\ntypedef uint32_t u32; typedef uint64_t u64; typedef uint32_t uid_t;\n#endif\n")
        (include / "bits.h").write_text("#ifndef BIT\n#define BIT(n) (1UL<<(n))\n#endif\n")
        (include / "string.h").write_text("#include <string.h>\n")
        for name in ["cred.h", "sched.h", "thread_info.h", "uidgid.h"]:
            (include / name).write_text("/* Host declarations precede project headers. */\n")
        for name in ["susfs_def.h", "ksu_susfs.h"]:
            (include / name).write_bytes((root / "include/linux" / name).read_bytes())
        lookup_flags = re.findall(r"^#define\s+LOOKUP_\w+\s+[^\n]+", (root / "include/linux/namei.h").read_text(), re.M)
        (output / "lookup_flags.h").write_text("\n".join(lookup_flags) + "\n")
        provider = (work / "kernel/feature/sucompat.c").read_text()
        adapted = function(provider, "ksu_devon_redirect_su_filename")
        adapted += function(provider, "ksu_handle_faccessat") + function(provider, "ksu_handle_stat")
        original = (ref / "kernel/feature/sucompat.c").read_text()
        pinned = function(original, "ksu_handle_faccessat") + function(original, "ksu_handle_stat")
        cc = ["clang", "-std=gnu11", "-O1", "-g", "-Wall", "-Wextra", "-Werror",
              "-Wno-unused-parameter", "-Wno-unused-function", "-Wno-unused-variable", "-Wno-sign-compare",
              "-Werror=incompatible-pointer-types", "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
              "-I", str(output)]
        enabled = ["-DCONFIG_KSU_SUSFS=1", "-DCONFIG_KSU=1", "-DCONFIG_KSU_DEVON_SELINUX_POLICY=1",
                   "-DCONFIG_ARM64=1", "-DCONFIG_64BIT=1", "-DCONFIG_THREAD_INFO_IN_TASK=1", "-D" + MACRO + "=1"]
        for name, flags in [("hooks-off", []), ("manual-common-path", ["-DCONFIG_KSU_MANUAL_HOOK=1"]),
                            ("susfs-static-key-0", enabled), ("susfs-static-key-1", enabled + ["-DKSU_COMPAT_USE_STATIC_KEY=1"]),
                            ("pinned-provider-negative", enabled + ["-DKSU_COMPAT_USE_STATIC_KEY=1", "-DPROVIDER_BASELINE_NEGATIVE=1"])]:
            (output / "provider_names.h").write_text(pinned if name.endswith("negative") else adapted)
            binary = output / name
            command = cc + flags + [str(fixture), "-o", str(binary)]
            compiled = subprocess.run(command, capture_output=True, text=True)
            (output / (name + "-compile.log")).write_text(compiled.stdout + compiled.stderr)
            assert not compiled.returncode, compiled.stderr
            result = subprocess.run([str(binary)], capture_output=True, text=True)
            (output / (name + ".log")).write_text(result.stdout + result.stderr)
            assert not result.returncode, result.stdout + result.stderr
            if name.endswith("negative"):
                assert "EXPECTED NEGATIVE" in result.stdout
                results["negative_control"] = result.stdout.strip()
            else:
                results["runs"].append({"name": name, "exit": 0, "result": result.stdout.strip()})
            print(name + ": " + result.stdout.strip())
        (output / "provider_names.h").write_text(adapted)
        # Preprocess the actual provider header independently of a filename
        # definition, ensuring it shares the global forward declaration.
        header = output / "bakasu_sucompat.h"
        header.write_bytes((work / "kernel/feature/sucompat.h").read_bytes())
        (output / "asm").mkdir(exist_ok=True)
        (output / "asm/ptrace.h").write_text("/* Host declaration-only check. */\n")
        (output / "compat").mkdir(exist_ok=True)
        (output / "compat/kernel_compat.h").write_text("/* Selection macros supplied explicitly. */\n")
        prototype_unit = output / "provider-header-check.c"
        prototype_unit.write_text("#include <stdbool.h>\n#include <stdint.h>\n#include \"linux/types.h\"\n"
            "struct task_struct;\ntypedef struct {uint32_t val;} kuid_t;\nkuid_t current_uid(void);\n"
            "bool test_thread_flag(unsigned);\nvoid set_thread_flag(unsigned);\nvoid clear_thread_flag(unsigned);\n"
            "void clear_tsk_thread_flag(struct task_struct *,unsigned);\n#define likely(v) (v)\n#define unlikely(v) (v)\n"
            "#include \"bakasu_sucompat.h\"\n"
            "int (*checked_access)(int *,struct filename **,int *,int *)=ksu_handle_faccessat;\n"
            "int (*checked_stat)(int *,struct filename **,int *)=ksu_handle_stat;\n")
        for key in [0, 1]:
            checked = subprocess.run(cc + enabled + (["-DKSU_COMPAT_USE_STATIC_KEY=1"] if key else []) +
                ["-fsyntax-only", str(prototype_unit)], capture_output=True, text=True)
            (output / ("provider-header-" + str(key) + ".log")).write_text(checked.stderr)
            assert not checked.returncode, checked.stderr
        results["provider_header"] = "actual header and shared contract agree for both static-key selections"
        # A userspace filename pointer is rejected by the shared public contract.
        for name, call in [("stat", "ksu_handle_stat(&dfd,&user_name,&flags)"),
                           ("access", "ksu_handle_faccessat(&dfd,&user_name,&mode,&flags)")]:
            unit = output / (name + "-wrong-prototype.c")
            unit.write_text(fixture.read_text() + "\nvoid wrong(void) {int dfd=0,mode=0,flags=0; const char *user_name=0; (void)" + call + ";}\n")
            result = subprocess.run(cc + enabled + ["-fsyntax-only", str(unit)], capture_output=True, text=True)
            (output / (name + "-wrong-prototype.log")).write_text(result.stderr)
            assert result.returncode and "incompatible pointer types" in result.stderr
        results["prototype_rejections"] = "two expected incompatible userspace-pointer compiler failures"
        results["provider_files"] = {name: digest(work / name) for name in PATCH_FILES}
    inputs = [patch, fixture, Path(__file__).resolve(), root / "scripts/susfs/test_selinux_autohooks.py",
              root / "scripts/susfs/test_selinux_adapter.py", root / "scripts/susfs/test_setresuid_task_state.py",
              root / "include/linux/ksu_susfs.h", root / "include/linux/susfs_def.h", root / "include/linux/namei.h",
              root / "fs/open.c", root / "fs/stat.c", root / "fs/namei.c", root / "include/linux/fs.h", root / "scripts/susfs/source_guards.py"]
    results["inputs"] = {str(p.relative_to(root)): digest(p) for p in inputs}
    (output / "results.json").write_text(json.dumps(results, indent=2) + "\n")
    print("Source/host checks passed; ARM64, real audit/VFS/credentials and kernel runtime remain unvalidated.")

if __name__ == "__main__":
    main()
