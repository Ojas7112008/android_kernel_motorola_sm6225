#!/usr/bin/env python3
"""Extracted reboot/input callers and BakaSU provider under host mocks.

No real reboot, kernel text patching, ARM64 build or device input runs here.
The SUSFS dispatcher and fd allocator are mocked; payload ABI is a later gate.
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
from test_setresuid_task_state import section


def main():
    root = Path(__file__).resolve().parents[2]
    ref = root / ".backport-context/repos/bakasu-sync-susfs"
    assert run(["git", "-C", str(ref), "rev-parse", "HEAD"]).strip() == PIN
    assert not run(["git", "-C", str(ref), "status", "--porcelain"])
    out = root / ".backport-context/audit/reboot-input"
    out.mkdir(parents=True, exist_ok=True)
    patch = root / ".github/patches/bakasu-devon-selinux-policy.patch"
    results = {"pin": PIN, "scope": __doc__, "compiler": run(["clang", "--version"]).splitlines()[0], "runs": []}
    with tempfile.TemporaryDirectory(prefix="devon-reboot-input-") as directory:
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
        reboot = (root / "kernel/reboot.c").read_text()
        old_reboot = kernel_source(root, 'kernel/reboot.c', legacy=True)
        start = reboot.index("#if defined(CONFIG_KSU_MANUAL_HOOK) && !defined(CONFIG_KSU_SUSFS)")
        end = reboot.index("\nstatic void deferred_cad", start)
        (out / "kernel_reboot.h").write_text("#ifdef CONFIG_KSU_SUSFS\n#include <linux/ksu_susfs.h>\n#endif\n" + reboot[start:end])
        # Outside declaration/call guards and their explanatory comment, native code is exact.
        restored = reboot.replace(reboot[start:end], old_reboot[old_reboot.index("#ifdef CONFIG_KSU_MANUAL_HOOK\nextern int ksu_handle_sys_reboot"):old_reboot.index("\nstatic void deferred_cad")], 1)
        restored = restored.replace("#ifdef CONFIG_KSU_SUSFS\n#include <linux/ksu_susfs.h>\n#endif\n", "", 1)
        assert restored == old_reboot
        for source in [reboot, old_reboot]:
            body = section(source, "SYSCALL_DEFINE4(reboot,")
            body = re.sub(r"#(?:if[^\n]*|endif)\n", "", body)
            body = re.sub(r"\s*/\* KSU replies[^*]+\*/", "", body)
            assert body.count("ksu_handle_sys_reboot(magic1, magic2, cmd, &arg);") == 1
        provider = (ref / "kernel/supercall/supercall.c").read_text()
        (out / "provider_reboot.h").write_text(function(provider, "ksu_handle_sys_reboot"))
        magic = (ref / "uapi/supercall.h").read_text()
        constants = re.findall(r"DEFINE_KSU_UAPI_CONST\(__u32, (KSU_INSTALL_MAGIC[12]), (0x[0-9A-F]+)\)", magic)
        assert len(constants) == 2
        (out / "reboot_magic.h").write_text("".join(f"#define {name} {value}U\n" for name, value in constants))
        (out / "native_reboot_constants.h").write_bytes((root / "include/uapi/linux/reboot.h").read_bytes())
        inp = (root / "drivers/input/input.c").read_text()
        old_inp = kernel_source(root, 'drivers/input/input.c', legacy=True)
        restored = inp.replace(function(inp, "input_handle_event").rstrip(), function(old_inp, "input_handle_event").rstrip(), 1)
        restored = restored.replace("\n#ifdef CONFIG_KSU_SUSFS\n#include <linux/ksu_susfs.h>\n#endif\n", "", 1)
        assert restored == old_inp, "unexpected input changes"
        assert function(inp, "input_handle_event").count("add_input_randomness(") == 1
        (out / "native_input.h").write_text("\n".join(function(inp, n) for n in
            ["is_event_supported", "input_get_disposition", "input_handle_event", "input_event"]))
        (out / "input_constants.h").write_text("\n".join(re.findall(r"^#define INPUT_(?:IGNORE_EVENT|PASS_TO_\w+|SLOT|FLUSH)[^\n]+", inp, re.M)) + "\n")
        (out / "input_event_codes.h").write_bytes((root / "include/uapi/linux/input-event-codes.h").read_bytes())
        runtime = (work / "kernel/runtime/ksud_integration.c").read_text()
        a = runtime.index("#ifdef KSU_COMPAT_DEVON_SELINUX_POLICY\n/* Input takes event_lock")
        b = runtime.index("#ifdef CONFIG_KSU_MANUAL_HOOK_AUTO_INPUT_HOOK\nstatic void vol_detector_event", a)
        adapted = runtime[a:b].replace("int ksu_handle_input_handle_event(", "int actual_input_hook(", 1)
        adapted += function(runtime, "ksu_stop_input_hook_runtime") + function(runtime, "ksu_is_safe_mode")
        (out / "provider_input.h").write_text(adapted)
        baseline = (ref / "kernel/runtime/ksud_integration.c").read_text()
        (out / "provider_input_baseline.h").write_text("static unsigned int volumedown_pressed_count;\n" +
            function(baseline, "is_volumedown_enough") + function(baseline, "ksu_handle_input_handle_event"))
        # Exercise the exact SUSFS static-key disable callback, not a test reimplementation.
        a = runtime.index("    static inline void stop_input_hook(void)", runtime.index("#elif defined(CONFIG_KSU_SUSFS)"))
        b = runtime.index("\n#elif defined(CONFIG_KSU_MANUAL_HOOK)", a)
        (out / "provider_stop_input.h").write_text(runtime[a:b])
        for name in ["userspace/ksud/src/android/susfs/api/susfsctl.rs", "userspace/ksud/src/android/ksucalls.rs"]:
            userspace = (ref / name).read_text()
            assert "SYS_reboot" in userspace and "KSU_INSTALL_MAGIC1" in userspace
        # Header contracts compile in the same TU as actual provider definitions.
        inc = out / "linux"
        inc.mkdir(exist_ok=True)
        (inc / "types.h").write_text("#pragma once\n#include <stdint.h>\n#include <sys/types.h>\n#define __user\ntypedef uint32_t u32;\n")
        (inc / "bits.h").write_text("#define BIT(n) (1UL<<(n))\n")
        (inc / "string.h").write_text("#include <string.h>\n")
        for name in ["cred.h", "sched.h", "thread_info.h", "uidgid.h"]:
            (inc / name).write_text("/* Fixture declarations precede project headers. */\n")
        for name in ["susfs_def.h", "ksu_susfs.h"]:
            (inc / name).write_bytes((root / "include/linux" / name).read_bytes())
        cc = ["clang", "-std=gnu11", "-O1", "-g", "-pthread", "-Wall", "-Wextra", "-Werror",
              "-Wno-unused-parameter", "-Wno-unused-function", "-Wno-unused-variable", "-Wno-sign-compare",
              "-Werror=incompatible-pointer-types", "-fsanitize=address,undefined", "-fno-omit-frame-pointer", "-I", str(out)]
        enabled = ["-DCONFIG_KSU_SUSFS=1", "-DCONFIG_KSU=1", "-DCONFIG_KSU_DEVON_SELINUX_POLICY=1",
                   "-DCONFIG_ARM64=1", "-DCONFIG_64BIT=1", "-DCONFIG_THREAD_INFO_IN_TASK=1", "-D" + MACRO + "=1"]
        matrix = [("hooks-off", []), ("manual-native", ["-DCONFIG_KSU_MANUAL_HOOK=1"]),
                  ("susfs-static-key-0", enabled), ("susfs-static-key-1", enabled + ["-DKSU_COMPAT_USE_STATIC_KEY=1"])]
        for mode, flags in matrix:
            for kind in ["reboot", "input"]:
                name = kind + "-" + mode
                binary = out / name
                compiled = run(cc + flags + [str(root / "scripts/susfs" / ("test_" + kind + ".c")), "-o", str(binary)])
                (out / (name + "-compile.log")).write_text(compiled)
                result = run([str(binary)])
                (out / (name + ".log")).write_text(result)
                print(name + ": " + result.strip())
                results["runs"].append({"name": name, "exit_code": 0, "output": result.strip()})
        probe = (root / "scripts/susfs/test_reboot.c").read_text().split("struct pid_namespace {")[0]
        for name, declaration in [("reject-reboot-user-pointer", "int ksu_handle_sys_reboot(int, int, unsigned int, void *);"),
                                  ("reject-input-value-pointer", "int ksu_handle_input_handle_event(unsigned int *, unsigned int *, long *);")]:
            rejected = subprocess.run(cc + enabled + ["-x", "c", "-fsyntax-only", "-"], input=probe + declaration + "\n", capture_output=True, text=True)
            (out / (name + ".log")).write_text(rejected.stderr)
            assert rejected.returncode and "conflicting types" in rejected.stderr
        results["interface_negatives"] = "wrong reboot indirection and input value types fail compilation"
        name = "pinned-input-repeat-negative"
        binary = out / name
        run(cc + enabled + ["-DPROVIDER_BASELINE_NEGATIVE=1", str(root / "scripts/susfs/test_input.c"), "-o", str(binary)])
        result = run([str(binary)])
        assert "EXPECTED NEGATIVE" in result
        (out / (name + ".log")).write_text(result)
        results["negative_control"] = result.strip()
        print(result.strip())
        results["provider_files"] = {n: digest(work / n) for n in PATCH_FILES}
    inputs = [patch, Path(__file__).resolve(), root / "scripts/susfs/test_reboot.c", root / "scripts/susfs/test_input.c",
              root / "scripts/susfs/test_selinux_autohooks.py", root / "scripts/susfs/test_selinux_adapter.py",
              root / "scripts/susfs/test_setresuid_task_state.py", root / "kernel/reboot.c", root / "drivers/input/input.c",
              root / "include/linux/ksu_susfs.h", root / "include/linux/susfs_def.h",
              root / "include/uapi/linux/reboot.h", root / "include/uapi/linux/input-event-codes.h",
              ref / "kernel/supercall/supercall.c", ref / "uapi/supercall.h",
              ref / "userspace/ksud/src/android/susfs/api/susfsctl.rs",
              ref / "userspace/ksud/src/android/ksucalls.rs"]
    results["inputs"] = {str(p.relative_to(root)): digest(p) for p in inputs}
    (out / "results.json").write_text(json.dumps(results, indent=2) + "\n")
    print("Host/source checks pass; full ARM64 build, actual dispatch/fd/input context and device behavior remain unvalidated.")


if __name__ == "__main__":
    main()
