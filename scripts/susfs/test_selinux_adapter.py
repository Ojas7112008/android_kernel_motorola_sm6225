#!/usr/bin/env python3
"""Host contract tests of extracted Motorola callers and patched BakaSU providers.

Historical f443 inline-adapter checkpoint only; the active port uses BakaSU automatic SELinux hooks.
This does not build KernelSU, relax its Kbuild stop, or test kernel/ARM64 runtime.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
from kernel_baseline import kernel_source
import subprocess
import tempfile

PIN = "f4435f007b16f61705089ff982e3f981a6017979"
PATCH_FILES = ["kernel/Kbuild", "kernel/feature/selinux_hide.c",
               "kernel/feature/selinux_hide.h", "kernel/selinux/rules.c",
               "kernel/selinux/selinux.h", "kernel/runtime/ksud_integration.c",
               "kernel/selinux/sepolicy.c"]


def run(cmd, cwd=None):
    result = subprocess.run(cmd, cwd=cwd, text=True, capture_output=True)
    if result.returncode:
        raise RuntimeError(f"{cmd}: exit {result.returncode}\n{result.stdout}{result.stderr}")
    return result.stdout


def function(source, name):
    match = re.search(r"^(?:static\s+)?(?:inline\s+)?(?:int|ssize_t|void|bool|u32|struct\s+\w+)\s+"
                      r"(?:__exit\s+)?\**\s*" + re.escape(name)
                      + r"\([^;{}]*?\)\n\{", source, re.M)
    if not match:
        raise ValueError("Function not found: " + name)
    start = match.start()
    brace = source.index("{", match.start())
    depth = 1
    end = brace + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end] + "\n"


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--bakasu", type=Path)
    parser.add_argument("--cc", default="clang")
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    ref = args.bakasu or root / ".backport-context/repos/bakasu"
    assert run(["git", "-C", str(ref), "rev-parse", "HEAD"]).strip() == PIN
    assert not run(["git", "-C", str(ref), "status", "--porcelain"])
    patch = root / ".github/patches/bakasu-devon-selinux-adapter-f443.patch"
    output = root / ".backport-context/audit/selinux-adapter-f443"
    output.mkdir(parents=True, exist_ok=True)
    results = {"bakasu_pin": PIN, "scope": "host mocked contract tests; no kernel build",
               "compiler": run([args.cc, "--version"]).splitlines()[0], "runs": []}
    with tempfile.TemporaryDirectory(prefix="devon-selinux-") as directory:
        work = Path(directory) / "bakasu"
        work.mkdir()
        # Reconstruct preserved kernel callers instead of testing the active auto-hook route.
        kernel = Path(directory) / "motorola"
        for name in ["security/selinux/hooks.c", "security/selinux/selinuxfs.c"]:
            dest = kernel / name
            dest.parent.mkdir(parents=True, exist_ok=True)
            dest.write_text(kernel_source(root, name))
        (kernel / "fs").mkdir()
        (kernel / "fs/exec.c").write_bytes((root / "fs/exec.c").read_bytes())
        run(["git", "init", "--quiet"], cwd=kernel)
        run(["git", "add", "--", "security"], cwd=kernel)
        checkpoint = root / "scripts/susfs/fixtures/f443-kernel-selinux.patch"
        run(["git", "apply", "--check", "--whitespace=error-all", str(checkpoint)], cwd=kernel)
        run(["git", "apply", "--whitespace=error-all", str(checkpoint)], cwd=kernel)
        for name in PATCH_FILES:
            dest = work / name
            dest.parent.mkdir(parents=True, exist_ok=True)
            dest.write_text(run(["git", "-C", str(ref), "show", PIN + ":" + name]))
        run(["git", "init", "--quiet"], cwd=work)
        run(["git", "add", "--", *PATCH_FILES], cwd=work)
        run(["git", "apply", "--check", "--whitespace=error-all", str(patch)], cwd=work)
        run(["git", "apply", "--whitespace=error-all", str(patch)], cwd=work)
        assert sorted(run(["git", "diff", "--name-only"], cwd=work).splitlines()) == sorted(PATCH_FILES)
        assert not run(["git", "ls-files", "--others", "--exclude-standard"], cwd=work)
        run(["git", "diff", "--check"], cwd=work)
        run(["git", "apply", "--reverse", "--check", str(patch)], cwd=work)
        results["patch_validation"] = "strict application, reverse check and reconstructed-source whitespace passed"
        build = (work / "kernel/Kbuild").read_text()
        start = build.index("  # Typed devon adapter;")
        end = build.index("  include $(KSU_SRC)/tools/susfs_compat.mk", start)
        guard = ref / "kernel/tools/susfs_compat.mk"
        assert digest(guard) == "25eb93fe03d155f5cb19f1de8a4e291d88f0016ff0f7e53b558f63fae1a9e2a7"
        fragment = ("srctree := " + str(kernel) + "\nREPO_NAME := BakaSU\n" + build[start:end]
                    + "$(info ADAPTER_FLAGS=$(ccflags-y))\ninclude " + str(guard)
                    + "\n.PHONY: all\nall:\n\t@:\n")
        gate = subprocess.run(["make", "--no-print-directory", "-f", "-", "all"],
                              input=fragment, text=True, capture_output=True)
        (output / "probe-and-guard.log").write_text(gate.stdout + gate.stderr)
        assert gate.returncode == 2 and "Please wait susfs update." in gate.stderr
        assert "KSU_COMPAT_SUSFS_DEVON_SELINUX_ADAPTER" in gate.stdout
        results["compatibility_guard"] = {"exit_code": 2, "sha256": digest(guard),
                                          "meaning": "expected upstream refusal; not a supported build"}
        hide = (work / "kernel/feature/selinux_hide.c").read_text()
        hooks = (kernel / "security/selinux/hooks.c").read_text()
        selinuxfs = (kernel / "security/selinux/selinuxfs.c").read_text()
        callers = function(hooks, "selinux_setprocattr")
        for name in ["sel_open_handle_status", "sel_read_handle_status", "sel_mmap_handle_status",
                     "sel_write_context", "sel_write_access"]:
            callers += function(selinuxfs, name)
        callers += "#ifdef CONFIG_KSU_SUSFS\n" + function(selinuxfs, "sel_release_handle_status") + "#endif\n"
        (output / "kernel_callers.h").write_text(callers)
        begin = hide.index("static inline void ksu_lock_sepolicy_legacy(void)")
        end = hide.index("/*\n * Write the security context", begin)
        (output / "bakasu_paired_parser.h").write_text(hide[begin:end])
        begin = hide.index("#ifdef KSU_COMPAT_SUSFS_DEVON_SELINUX_ADAPTER\n/* Readers")
        providers = hide[begin:]
        for name in ["selinux_hide_feature_set", "ksu_selinux_hide_exit",
                     "ksu_selinux_hide_drop_backup_if_unused"]:
            providers += function(hide, name)
        (output / "bakasu_providers.h").write_text(providers)
        rules = (work / "kernel/selinux/rules.c").read_text()
        begin = rules.index("#ifdef KSU_COMPAT_SUSFS_DEVON_SELINUX_ADAPTER\n/* Protected")
        end = rules.index("#else\nvoid apply_kernelsu_rules()", begin)
        lifecycle = function(hide, "ksu_selinux_hide_lock") + function(hide, "ksu_selinux_hide_unlock")
        lifecycle += function(rules, "ksu_apply_default_rules") + rules[begin:end] + "#endif\n"
        lifecycle += function(rules, "handle_sepolicy")
        lifecycle += function(hide, "ksu_selinux_hide_drop_backup_if_unused")
        (output / "bakasu_policy_lifecycle.h").write_text(lifecycle)
        serialization = function((work / "kernel/selinux/sepolicy.c").read_text(), "ksu_dup_policydb")
        (output / "bakasu_policy_copy.h").write_text(serialization)
        boot = function((work / "kernel/runtime/ksud_integration.c").read_text(), "ksu_handle_execveat_ksud")
        (output / "bakasu_policy_boot.h").write_text(boot)
        api = (work / "kernel/selinux/selinux.h").read_text()
        begin = api.index("#ifdef KSU_COMPAT_SUSFS_DEVON_SELINUX_ADAPTER\nint apply_kernelsu_rules")
        end = api.index("#endif", begin) + len("#endif")
        (output / "bakasu_policy_api.h").write_text(api[begin:end] + "\n")
        def no_includes(text):
            return re.sub(r"^#include[^\n]*\n", "", text, flags=re.M)

        for source, dest in [("include/linux/flex_array.h", "vendor_flex_types.h"),
                             ("include/linux/reciprocal_div.h", "vendor_reciprocal_types.h"),
                             ("lib/flex_array.c", "vendor_flex_array.h"),
                             ("security/selinux/ss/ebitmap.h", "vendor_ebitmap_types.h"),
                             ("security/selinux/ss/hashtab.h", "vendor_hashtab_types.h"),
                             ("security/selinux/ss/symtab.h", "vendor_symtab_types.h"),
                             ("security/selinux/ss/hashtab.c", "vendor_hashtab.h")]:
            (output / dest).write_text(no_includes((root / source).read_text()))
        (output / "vendor_reciprocal.h").write_text(function((root / "lib/reciprocal_div.c").read_text(), "reciprocal_value"))
        bitmaps = (root / "security/selinux/ss/ebitmap.c").read_text()
        (output / "vendor_ebitmap.h").write_text("".join(function(bitmaps, n) for n in
            ["ebitmap_cmp", "ebitmap_cpy", "ebitmap_get_bit", "ebitmap_set_bit", "ebitmap_destroy"]))
        policy_types = (root / "security/selinux/ss/policydb.h").read_text()
        types = ""
        for name in ["type_datum", "role_datum"]:
            match = re.search(r"struct " + name + r"\s*\{[^}]+\};", policy_types)
            assert match, name
            types += match.group(0) + "\n"
        (output / "vendor_policy_types.h").write_text(types)
        type_source = (work / "kernel/selinux/sepolicy.c").read_text()
        begin = type_source.index("#ifdef KSU_COMPAT_SUSFS_DEVON_SELINUX_ADAPTER\n#if defined(KSU_COMPAT_HAS_MODERN_POLICYDB)")
        end = type_source.index("#endif /* KSU_COMPAT_SUSFS_DEVON_SELINUX_ADAPTER */", begin)
        end += len("#endif /* KSU_COMPAT_SUSFS_DEVON_SELINUX_ADAPTER */")
        creation = type_source[begin:end] + "\n" + function(type_source, "add_type")
        baseline_types = run(["git", "-C", str(ref), "show", PIN + ":kernel/selinux/sepolicy.c"])
        (output / "bakasu_baseline_type_creation.h").write_text(function(baseline_types, "add_type"))
        (output / "bakasu_type_creation.h").write_text(creation)
        include = output / "linux"
        include.mkdir(exist_ok=True)
        (include / "types.h").write_text("#include <stdbool.h>\n#include <stdint.h>\ntypedef uint32_t u32; typedef uint16_t u16; typedef uint64_t u64;\n")
        (output / "susfs_selinux.h").write_bytes((kernel / "include/linux/susfs_selinux.h").read_bytes())
        fixture = root / "scripts/susfs/test_selinux_adapter.c"
        for enabled, static_key in [(0, 0), (1, 0), (1, 1)]:
            name = f"susfs-{enabled}-static-key-{static_key}"
            binary = output / name
            cmd = [args.cc, "-std=gnu11", "-O1", "-g", "-pthread", "-Wall", "-Wextra", "-Werror",
                   "-Wno-unused-parameter", "-Wno-unused-function", "-Wno-unused-variable",
                   "-Werror=incompatible-pointer-types", "-fsanitize=address,undefined",
                   "-fno-omit-frame-pointer", "-I", str(output), str(fixture), "-o", str(binary)]
            if enabled:
                cmd += ["-DCONFIG_KSU=1", "-DCONFIG_KSU_SUSFS=1",
                        "-DKSU_COMPAT_SUSFS_DEVON_SELINUX_ADAPTER=1"]
            if static_key:
                cmd += ["-DKSU_COMPAT_USE_STATIC_KEY=1"]
            compiled = run(cmd)
            (output / (name + "-compile.log")).write_text(compiled)
            result = run([str(binary)])
            (output / (name + ".log")).write_text(result)
            print(name + ": " + result.strip())
            results["runs"].append({"name": name, "exit_code": 0, "output": result.strip()})
        lifecycle_fixture = root / "scripts/susfs/test_selinux_policy_lifecycle.c"
        for init_path in range(3):
            name = f"policy-lifecycle-init-{init_path}"
            binary = output / name
            cmd = [args.cc, "-std=gnu11", "-O1", "-g", "-pthread", "-Wall", "-Wextra", "-Werror",
                   "-Wno-unused-parameter", "-Wno-unused-function", "-Wno-unused-variable",
                   "-fsanitize=address,undefined", "-fno-omit-frame-pointer", "-I", str(output),
                   "-DKSU_COMPAT_SUSFS_DEVON_SELINUX_ADAPTER=1", f"-DINIT_PATH={init_path}",
                   str(lifecycle_fixture), "-o", str(binary)]
            compile_result = run(cmd)
            (output / (name + "-compile.log")).write_text(compile_result)
            result = run([str(binary)])
            (output / (name + ".log")).write_text(result)
            print(name + ": " + result.strip())
            results["runs"].append({"name": name, "exit_code": 0, "output": result.strip()})
        type_fixture = root / "scripts/susfs/test_selinux_type_creation.c"
        name = "type-creation-vendor-primitives"
        binary = output / name
        cmd = [args.cc, "-std=gnu11", "-O1", "-g", "-Wall", "-Wextra", "-Werror",
               "-Wno-unused-parameter", "-Wno-unused-function", "-Wno-unused-variable",
               "-Wno-sign-compare", "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
               "-I", str(output), "-DKSU_COMPAT_SUSFS_DEVON_SELINUX_ADAPTER=1",
               str(type_fixture), "-o", str(binary)]
        compiled = run(cmd)
        (output / (name + "-compile.log")).write_text(compiled)
        result = run([str(binary)])
        (output / (name + ".log")).write_text(result)
        print(name + ": " + result.strip())
        results["runs"].append({"name": name, "exit_code": 0, "output": result.strip()})
        baseline_binary = output / "type-creation-baseline-negative"
        baseline_cmd = [c for c in cmd if c != "-DKSU_COMPAT_SUSFS_DEVON_SELINUX_ADAPTER=1"]
        baseline_cmd[baseline_cmd.index("-o") + 1] = str(baseline_binary)
        baseline_cmd += ["-DBASELINE_TYPE_CREATION=1"]
        (output / "type-creation-baseline-compile.log").write_text(run(baseline_cmd))
        negative = run([str(baseline_binary)])
        (output / "type-creation-baseline-negative.log").write_text(negative)
        assert "EXPECTED NEGATIVE" in negative
        results["baseline_type_failure"] = negative.strip()
        print(negative.strip())
        for macro in ["KSU_COMPAT_HAS_MODERN_POLICYDB", "KSU_COMPAT_TYPE_ATTR_MAP_ARRAY_NOT_FOUND"]:
            rejection_cmd = cmd[:cmd.index("-o")] + ["-fsyntax-only", "-D" + macro + "=1"]
            rejection = subprocess.run(rejection_cmd, capture_output=True, text=True)
            (output / (macro + "-rejection.log")).write_text(rejection.stderr)
            assert rejection.returncode and "requires the audited flex-array policydb layout" in rejection.stderr
        results["type_layout_rejection"] = "two expected compiler failures"
        # An incomplete/module integration must fail rather than supply stubs.
        rejection = subprocess.run([args.cc, "-I", str(output), "-DCONFIG_KSU_SUSFS=1",
                                    "-x", "c", "-fsyntax-only", "-"],
                                   input='#include "susfs_selinux.h"\n', text=True, capture_output=True)
        (output / "module-rejection.log").write_text(rejection.stderr)
        assert rejection.returncode and "requires built-in KernelSU" in rejection.stderr
        results["module_rejection"] = "expected compile failure"
        results["patched_files"] = {name: digest(work / name) for name in PATCH_FILES}
    inputs = [patch, root / "scripts/susfs/fixtures/f443-kernel-selinux.patch", Path(__file__).resolve(), fixture, lifecycle_fixture, type_fixture,
              *[root / n for n in ["lib/flex_array.c", "include/linux/flex_array.h",
                                  "lib/reciprocal_div.c", "include/linux/reciprocal_div.h",
                                  "security/selinux/ss/ebitmap.c", "security/selinux/ss/ebitmap.h",
                                  "security/selinux/ss/hashtab.c", "security/selinux/ss/hashtab.h",
                                  "security/selinux/ss/symtab.h", "security/selinux/ss/policydb.h"]]]
    results["inputs"] = {str(p.relative_to(root)): digest(p) for p in inputs}
    (output / "results.json").write_text(json.dumps(results, indent=2) + "\n")
    print("All host contract checks passed. Kernel compile, lockdep and device validation remain untested.")


if __name__ == "__main__":
    main()
