#!/usr/bin/env python3
"""Extract and validate devon UID/task-state code under host mocks and sanitizers.

This reconstructs the entire provider patch. It does not build or boot a kernel,
run real seccomp/VFS operations, or prove complete SUSFS command compatibility.
"""
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

def section(text, marker):
    start = text.index(marker)
    pos = text.index("{", start)
    level = 1
    end = pos + 1
    while level:
        level += (text[end] == "{") - (text[end] == "}")
        end += 1
    return text[start:end] + "\n"

def main():
    root = Path(__file__).resolve().parents[2]
    ref = root / ".backport-context/repos/bakasu-sync-susfs"
    assert run(["git", "-C", str(ref), "rev-parse", "HEAD"]).strip() == PIN
    assert not run(["git", "-C", str(ref), "status", "--porcelain"])
    output = root / ".backport-context/audit/setresuid-task-state"
    output.mkdir(parents=True, exist_ok=True)
    patch = root / ".github/patches/bakasu-devon-selinux-policy.patch"
    fixture = root / "scripts/susfs/test_setresuid_task_state.c"
    results = {"pin": PIN, "scope": __doc__, "compiler": run(["clang", "--version"]).splitlines()[0], "runs": []}
    with tempfile.TemporaryDirectory(prefix="devon-uid-") as temporary:
        work = Path(temporary) / "bakasu"
        for name in PATCH_FILES:
            path = work / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(run(["git", "-C", str(ref), "show", PIN + ":" + name]))
        run(["git", "init", "--quiet"], cwd=work)
        run(["git", "add", "--", *PATCH_FILES], cwd=work)
        run(["git", "apply", "--check", "--whitespace=error-all", str(patch)], cwd=work)
        run(["git", "apply", "--whitespace=error-all", str(patch)], cwd=work)
        run(["git", "diff", "--check"], cwd=work)
        run(["git", "apply", "--reverse", "--check", str(patch)], cwd=work)
        assert sorted(run(["git", "diff", "--name-only"], cwd=work).splitlines()) == sorted(PATCH_FILES)
        # The upstream 2.3 definitions are preserved; only standalone includes
        # and the target storage guard are added to this checkpoint.
        upstream_defs = (root / ".backport-context/repos/susfs/kernel_patches/include/linux/susfs_def.h").read_text()
        actual_defs = (root / "include/linux/susfs_def.h").read_text()
        extras = "#include <linux/sched.h>\n#include <linux/thread_info.h>\n#include <linux/uidgid.h>\n\n#if defined(CONFIG_KSU_SUSFS) && (!defined(CONFIG_64BIT) || !defined(CONFIG_THREAD_INFO_IN_TASK))\n#error \"devon SUSFS task flags require 64-bit THREAD_INFO_IN_TASK storage\"\n#endif\n"
        assert actual_defs.count("#include <linux/cred.h>\n") == 1
        assert "#include <linux/cred.h>\n" not in upstream_defs
        assert actual_defs.replace(extras, "").replace("#include <linux/cred.h>\n", "", 1) == upstream_defs
        commit_source = function((root / "kernel/cred.c").read_text(), "commit_creds")
        assert re.findall(r"\breturn\s+([^;]+);", commit_source) == ["0"]
        kernel = (root / "kernel/sys.c").read_text()
        caller = section(kernel, "long __sys_setresuid(")
        assert caller.index("security_task_fix_setuid") < caller.index("ksu_handle_setresuid") < caller.index("return commit_creds(new)")
        (output / "kernel_uid_caller.h").write_text(caller)
        (output / "kernel_uid16_caller.h").write_text(section((root / "kernel/uid16.c").read_text(), "SYSCALL_DEFINE3(setresuid16,"))
        native_flags = (root / "arch/arm64/include/asm/thread_info.h").read_text()
        native_flags = native_flags[native_flags.index("#define TIF_SIGPENDING"):native_flags.index("#define INIT_THREAD_INFO")]
        (output / "vendor_thread_flags.h").write_text(native_flags)
        fork = (root / "kernel/fork.c").read_text()
        assert fork.index("setup_thread_stack(tsk, orig);") < fork.index("ksu_susfs_reset_fork_state(tsk);") < fork.index("clear_user_return_notifier(tsk);")
        (output / "fork-selection.txt").write_text("Hook follows native task/thread-state copying and precedes child publication. Only bit 61 is cleared; restrictions inherit.\n")
        core = (work / "kernel/core/init.c").read_text()
        selinux = (work / "kernel/selinux/selinux.c").read_text()
        credential = function(selinux, "transive_to_domain") + function(selinux, "setup_ksu_cred_selinux")
        credential += "static bool ksu_devon_cred_ready;\nstatic DEFINE_MUTEX(ksu_devon_cred_mutex);\n"
        credential += function(core, "ksu_susfs_cred_ready") + function(core, "setup_ksu_cred")
        (output / "bakasu_credential_setup.h").write_text(credential)
        uid = (work / "kernel/hook/setuid_hook.c").read_text()
        provider = "".join(function(uid, name) for name in ["handle_zygote_next_setresuid", "ksu_handle_setuid", "ksu_handle_setresuid"])
        (output / "bakasu_uid_provider.h").write_text(provider)
        allowlist = (ref / "kernel/policy/allowlist.h").read_text()
        (output / "bakasu_uid_classifiers.h").write_text(function(allowlist, "is_appuid") + function(allowlist, "is_isolated_process"))
        exec_source = (work / "kernel/feature/sucompat.c").read_text()
        exec_state = "".join(function(exec_source, name) for name in ["do_ksu_handle_execveat_sucompat", "ksu_handle_post_execve", "ksu_handle_execve"])
        (output / "bakasu_exec_state.h").write_text(exec_state)
        baseline_exec = (ref / "kernel/feature/sucompat.c").read_text()
        (output / "bakasu_baseline_exec_state.h").write_text("".join(function(baseline_exec, name) for name in
            ["do_ksu_handle_execveat_sucompat", "ksu_handle_post_execve", "ksu_handle_execve"]))
        include = output / "linux"
        include.mkdir(exist_ok=True)
        (include / "types.h").write_text("#ifndef UID_TEST_TYPES\n#define UID_TEST_TYPES\n#include <stdint.h>\n#include <sys/types.h>\n#ifndef __user\n#define __user\n#endif\ntypedef uint32_t u32; typedef uint32_t uid_t;\n#endif\n")
        (include / "bits.h").write_text("#ifndef BIT\n#define BIT(n) (1UL<<(n))\n#endif\n")
        (include / "string.h").write_text("#include <string.h>\n")
        for name in ["cred.h", "sched.h", "thread_info.h", "uidgid.h"]:
            (include / name).write_text("/* Host declarations precede actual project headers. */\n")
        for name in ["susfs_def.h", "ksu_susfs.h"]:
            (include / name).write_bytes((root / "include/linux" / name).read_bytes())
        # Recover the actual legacy early hook as a semantic negative control.
        legacy = Path(temporary) / "legacy"
        (legacy / "kernel").mkdir(parents=True)
        (legacy / "kernel/sys.c").write_text(kernel_source(root, 'kernel/sys.c', legacy=True))
        run(["git", "init", "--quiet"], cwd=legacy)
        run(["git", "add", "--", "kernel/sys.c"], cwd=legacy)
        run(["git", "apply", "--include=kernel/sys.c", str(root / "scripts/susfs/fixtures/legacy-sys-susfs-1.5.9.patch")], cwd=legacy)
        (output / "kernel_early_uid_caller.h").write_text(section((legacy / "kernel/sys.c").read_text(), "long __sys_setresuid("))
        cc = ["clang", "-std=gnu11", "-O1", "-g", "-pthread", "-Wall", "-Wextra", "-Werror",
              "-Wno-unused-parameter", "-Wno-unused-function", "-Wno-unused-variable",
              "-Werror=incompatible-pointer-types", "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
              "-I", str(output)]
        enabled = ["-DCONFIG_KSU_SUSFS=1", "-DCONFIG_KSU=1", "-DCONFIG_KSU_DEVON_SELINUX_POLICY=1",
                   "-DCONFIG_ARM64=1", "-DCONFIG_64BIT=1", "-DCONFIG_THREAD_INFO_IN_TASK=1", "-D" + MACRO + "=1"]
        for name, flags in [("ksu-off", []), ("susfs-static-key-0", enabled),
                            ("susfs-static-key-1", enabled + ["-DKSU_COMPAT_USE_STATIC_KEY=1"]),
                            ("legacy-early-hook-negative", enabled + ["-DUID_EARLY_NEGATIVE=1"]),
                            ("pinned-exec-negative", enabled + ["-DEXEC_BASELINE_NEGATIVE=1"])]:
            binary = output / name
            command = cc + flags + [str(fixture), "-o", str(binary)]
            (output / (name + "-compile.log")).write_text(run(command))
            result = run([str(binary)])
            (output / (name + ".log")).write_text(result)
            if name.endswith("negative"):
                assert "EXPECTED NEGATIVE" in result
                results.setdefault("negative_controls", {})[name] = result.strip()
            else:
                results["runs"].append({"name": name, "exit": 0, "result": result.strip()})
            print(name + ": " + result.strip())
        for remove, message in [("CONFIG_KSU", "built-in ARM64 KSU"), ("CONFIG_64BIT", "64-bit THREAD_INFO_IN_TASK"),
                                ("CONFIG_THREAD_INFO_IN_TASK", "64-bit THREAD_INFO_IN_TASK"),
                                ("CONFIG_KSU_DEVON_SELINUX_POLICY", "checked policy path")]:
            flags = [flag for flag in enabled if flag != "-D" + remove + "=1"]
            rejected = subprocess.run(cc + flags + ["-fsyntax-only", str(fixture)], capture_output=True, text=True)
            assert rejected.returncode and message in rejected.stderr
            (output / (remove + "-rejection.log")).write_text(rejected.stderr)
        results["configuration_rejections"] = "four expected compiler failures"
        results["provider_files"] = {name: digest(work / name) for name in PATCH_FILES}
    inputs = [patch, fixture, Path(__file__).resolve(), root / "scripts/susfs/test_selinux_autohooks.py",
              root / "scripts/susfs/test_selinux_adapter.py", root / "include/linux/ksu_susfs.h",
              root / "include/linux/susfs_def.h", root / "kernel/sys.c", root / "kernel/fork.c",
              root / "kernel/uid16.c", root / "kernel/cred.c", root / "fs/exec.c",
              root / "arch/arm64/include/asm/thread_info.h"]
    results["inputs"] = {str(p.relative_to(root)): digest(p) for p in inputs}
    (output / "results.json").write_text(json.dumps(results, indent=2) + "\n")
    print("All host/source checks passed; kernel credential, seccomp, VFS, scheduler and device behavior remain unvalidated.")

if __name__ == "__main__":
    main()
