#!/usr/bin/env python3
"""Host/source validation of the pinned BakaSU automatic SELinux route for devon.

Tests extract actual patched BakaSU functions and stock Motorola callers. They model function-table patching but do
not execute kernel text patching, build ARM64 objects, or establish boot safety.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import tempfile
from test_selinux_adapter import function, run, digest

PIN = "e1682a2831142e68fa30bab97d5690cf90b75119"
MACRO = "KSU_COMPAT_DEVON_SELINUX_POLICY"
PATCH_FILES = ["kernel/Kbuild", "kernel/Kconfig", "kernel/feature/selinux_hide.c",
               "kernel/feature/selinux_hide.h", "kernel/selinux/rules.c",
               "kernel/selinux/selinux.h", "kernel/runtime/ksud_integration.c",
               "kernel/selinux/sepolicy.c", "kernel/tools/kernel_compat.mk",
               "kernel/core/init.c", "kernel/include/ksu.h", "kernel/selinux/selinux.c",
               "kernel/hook/setuid_hook.c", "kernel/hook/setuid_hook.h",
               "kernel/feature/sucompat.c", "kernel/feature/sucompat.h"]

def gate(fragment, output, name):
    result = subprocess.run(["make", "--no-print-directory", "-f", "-", "all"],
                            input=fragment + "\n.PHONY: all\nall:\n\t@:\n",
                            capture_output=True, text=True)
    (output / (name + ".log")).write_text(result.stdout + result.stderr)
    return result

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cc", default="clang")
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    pins = json.loads((root / "scripts/susfs/source-pins.json").read_text())
    assert pins["bakasu"] == PIN
    assert pins["bakasu_patch_files"] == len(PATCH_FILES)
    ref = root / ".backport-context/repos/bakasu-sync-susfs"
    assert run(["git", "-C", str(ref), "rev-parse", "HEAD"]).strip() == PIN
    assert not run(["git", "-C", str(ref), "status", "--porcelain"])
    # Native Motorola SELinux operations belong to BakaSU's automatic route.
    for name in ["security/selinux/hooks.c", "security/selinux/selinuxfs.c"]:
        assert (root / name).read_text() == run(["git", "-C", str(root), "show", pins["kernel"] + ":" + name])
    patch = root / pins["bakasu_patch"]
    output = root / ".backport-context/audit/selinux-autohooks"
    output.mkdir(parents=True, exist_ok=True)
    results = {"bakasu_pin": PIN, "scope": __doc__, "compiler": run([args.cc, "--version"]).splitlines()[0], "runs": []}
    with tempfile.TemporaryDirectory(prefix="devon-auto-selinux-") as directory:
        work = Path(directory)
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
        results["patch_validation"] = "strict apply/reverse checks and reconstructed-source whitespace passed"
        build = (work / "kernel/Kbuild").read_text()
        begin = build.index("ifdef CONFIG_KSU_DEVON_SELINUX_POLICY")
        end = build.index("ifdef CONFIG_KSU_TRACEPOINT_HOOK", begin)
        selection = build[begin:end]
        guard = ref / "kernel/tools/susfs_compat.mk"
        assert guard.read_text() == run(["git", "-C", str(ref), "show", PIN + ":kernel/tools/susfs_compat.mk"])
        assert "KSU_COMPAT_HAS_SUSFS_FEATURE_SELINUX_HIDE" not in selection
        assert "KSU = y && KSU_SUSFS && ARM64 && KALLSYMS_ALL" in (work / "kernel/Kconfig").read_text()
        common = "srctree := " + str(root) + "\nREPO_NAME := BakaSU\nVERSION := 4\nPATCHLEVEL := 19\n"
        config = "CONFIG_KSU := y\nCONFIG_KSU_SUSFS := y\nCONFIG_KSU_DEVON_SELINUX_POLICY := y\n"
        fragment = common + config + selection + "include " + str(guard) + "\n$(info FLAGS=$(ccflags-y))\n"
        accepted = gate(fragment, output, "selection-and-upstream-guard")
        assert accepted.returncode == 0 and MACRO in accepted.stdout
        assert "KSU_COMPAT_HAS_SUSFS_FEATURE_SELINUX_HIDE" not in accepted.stdout
        uppercase = gate(fragment.replace("CONFIG_KSU := y", "CONFIG_KSU := Y"), output, "selection-uppercase-builtin")
        assert uppercase.returncode == 0 and MACRO in uppercase.stdout
        for key, value in [("CONFIG_KSU", "m"), ("CONFIG_KSU", "M"), ("CONFIG_KSU_SUSFS", "n"), ("PATCHLEVEL", "14")]:
            rejected = gate(fragment.replace(key + " := " + ("19" if key == "PATCHLEVEL" else "y"), key + " := " + value), output, "reject-" + key)
            assert rejected.returncode == 2
        inline = gate(common + "include " + str(ref / "kernel/tools/inline_hook_check.mk"), output, "remaining-inline-hook-check")
        assert inline.returncode == 0 and "ksu_handle_input_handle_event found" in inline.stdout
        assert "WARNING: Detected KSU_MANUAL_HOOK guard" in inline.stdout
        results["compatibility"] = {"fragment_exit": 0, "guard_sha256": digest(guard),
            "invalid_modes": "three expected make failures", "remaining_inline_hook_check": 0,
            "meaning": "compatibility fragment and unchanged hook-name check pass; manual-guard warnings retained; names alone do not establish core/ABI/build compatibility"}
        api_probe = gate(common + "objtree := " + str(root) + "\ninclude " + str(work / "kernel/tools/kernel_compat.mk")
                         + "\n$(info FLAGS=$(ccflags-y))", output, "actual-kernel-api-probes")
        assert api_probe.returncode == 0
        actual_flags = re.search(r"^FLAGS=(.*)$", api_probe.stdout, re.M).group(1).split()
        for required in ["KSU_COMPAT_HAS_SELINUX_STATE", "KSU_COMPAT_SIDTAB_AS_REFERENCE",
                         "KSU_COMPAT_HAS_STRICTER_MLS_CONTEXT_TO_SID", "KSU_COMPAT_HAS_CONTEXT_ADD_HASH"]:
            assert "-D" + required in actual_flags
        for incompatible in ["KSU_COMPAT_HAS_SELINUX_POLICY_STRUCT", "KSU_COMPAT_HAS_POLICY_MUTEX"]:
            assert "-D" + incompatible not in actual_flags
        results["actual_api_probe_flags"] = [f for f in actual_flags if f.startswith("-D")]
        hide = (work / "kernel/feature/selinux_hide.c").read_text()
        baseline_hide = (ref / "kernel/feature/selinux_hide.c").read_text()
        start = hide.index("#ifdef KSU_COMPAT_DEVON_SELINUX_POLICY")
        end = hide.index("static DEFINE_MUTEX", start)
        layout = re.sub(r"^#include[^\n]*\n", "", hide[start:end], flags=re.M)
        flags = ["-D" + MACRO + "=1", "-DKSU_COMPAT_USE_SELINUX_STATE=1", "-DKSU_COMPAT_SIDTAB_AS_REFERENCE=1"]
        prelude = "#define KERNEL_VERSION(a,b,c) (((a)<<16)+((b)<<8)+(c))\n#define LINUX_VERSION_CODE KERNEL_VERSION(4,19,255)\n"
        for forbidden in [False, True]:
            rejection = subprocess.run([args.cc, "-x", "c", "-fsyntax-only", "-", *flags,
                                        *(["-DKSU_COMPAT_HAS_SUSFS_FEATURE_SELINUX_HIDE=1"] if forbidden else [])],
                                       input=prelude + layout, capture_output=True, text=True)
            (output / ("nongki-rejection.log" if forbidden else "nongki-selection.log")).write_text(rejection.stderr)
            assert bool(rejection.returncode) == forbidden
            if forbidden:
                assert "BakaSU does NOT support susfs selinux_hide" in rejection.stderr
        api_header = re.sub(r"^#include[^\n]*\n", "", (work / "kernel/selinux/selinux.h").read_text(), flags=re.M)
        selected = subprocess.run([args.cc, "-x", "c", "-E", "-dM", "-", "-D" + MACRO + "=1",
                                   *[f for f in actual_flags if f.startswith("-D")]],
                                  input=prelude + api_header + layout, capture_output=True, text=True)
        (output / "actual-api-layout-selection.log").write_text(selected.stdout + selected.stderr)
        assert selected.returncode == 0 and "#define KSU_COMPAT_USE_SELINUX_STATE" in selected.stdout
        results["nongki_error"] = "upstream error preserved and exercised; selected auto path passes"
        hooks = (root / "security/selinux/hooks.c").read_text()
        selinuxfs = (root / "security/selinux/selinuxfs.c").read_text()
        callers = function(hooks, "selinux_setprocattr")
        for name in ["sel_open_handle_status", "sel_read_handle_status", "sel_mmap_handle_status", "sel_write_context", "sel_write_access"]:
            callers += function(selinuxfs, name)
        (output / "kernel_stock_callers.h").write_text(callers)
        begin = hide.index("static inline void ksu_lock_sepolicy_legacy(void)")
        end = hide.index("/*\n * Write the security context", begin)
        auto = hide[begin:end]
        for name in ["ksu_selinux_hide_lock", "ksu_selinux_hide_unlock", "ksu_devon_auto_begin"]:
            auto += function(hide, name)
        begin = hide.index("static ssize_t my_write_context_inner")
        end = hide.index("/*\n * get the security ID", begin)
        auto += hide[begin:end]
        begin = hide.index("#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0)", hide.index("static setprocattr_fn ksu_orig_setprocattr;"))
        end = hide.index("typedef int (*sel_open_handle_status_fn)", begin)
        auto += hide[begin:end]
        auto += re.search(r"^typedef int \(\*sel_open_handle_status_fn\)[^;]+;", hide, re.M).group(0) + "\n"
        for name in ["ksu_devon_status_release", "my_sel_open_handle_status", "hook_selinux_status_open", "selinux_hide_feature_set", "ksu_selinux_hide_exit", "ksu_selinux_hide_drop_backup_if_unused"]:
            auto += function(hide, name)
        (output / "bakasu_auto_functions.h").write_text(auto)
        negative = function(baseline_hide, "my_sel_open_handle_status").replace("my_sel_open_handle_status(", "baseline_my_sel_open_handle_status(")
        (output / "bakasu_baseline_status.h").write_text(negative)
        rules = (work / "kernel/selinux/rules.c").read_text()
        begin = rules.index("#ifdef " + MACRO + "\n/* Protected")
        end = rules.index("#else\nvoid apply_kernelsu_rules()", begin)
        lifecycle = function(hide, "ksu_selinux_hide_lock") + function(hide, "ksu_selinux_hide_unlock")
        lifecycle += function(rules, "ksu_apply_default_rules") + rules[begin:end] + "#endif\n"
        lifecycle += function(rules, "handle_sepolicy") + function(hide, "ksu_selinux_hide_drop_backup_if_unused")
        (output / "bakasu_policy_lifecycle.h").write_text(lifecycle)
        (output / "bakasu_policy_copy.h").write_text(function((work / "kernel/selinux/sepolicy.c").read_text(), "ksu_dup_policydb"))
        (output / "bakasu_policy_boot.h").write_text(function((work / "kernel/runtime/ksud_integration.c").read_text(), "ksu_handle_execveat_ksud"))
        api = (work / "kernel/selinux/selinux.h").read_text()
        begin = api.index("#ifdef " + MACRO + "\nint apply_kernelsu_rules")
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
        begin = type_source.index("#ifdef KSU_COMPAT_DEVON_SELINUX_POLICY\n#if defined(KSU_COMPAT_HAS_MODERN_POLICYDB)")
        end = type_source.index("#endif /* KSU_COMPAT_DEVON_SELINUX_POLICY */", begin)
        end += len("#endif /* KSU_COMPAT_DEVON_SELINUX_POLICY */")
        creation = type_source[begin:end] + "\n" + function(type_source, "add_type")
        baseline_types = run(["git", "-C", str(ref), "show", PIN + ":kernel/selinux/sepolicy.c"])
        (output / "bakasu_baseline_type_creation.h").write_text(function(baseline_types, "add_type"))
        (output / "bakasu_type_creation.h").write_text(creation)
        include = output / "linux"
        include.mkdir(exist_ok=True)
        (include / "types.h").write_text("#include <stdbool.h>\n#include <stdint.h>\ntypedef uint32_t u32; typedef uint16_t u16; typedef uint64_t u64;\n")
        common_cc = [args.cc, "-std=gnu11", "-O1", "-g", "-pthread", "-Wall", "-Wextra", "-Werror",
                     "-Wno-unused-parameter", "-Wno-unused-function", "-Wno-unused-variable", "-Wno-sign-compare",
                     "-Werror=incompatible-pointer-types", "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
                     "-I", str(output), "-D" + MACRO + "=1"]
        tests = [("automatic-static-key-0", "test_selinux_autohooks.c", []),
                 ("automatic-static-key-1", "test_selinux_autohooks.c", ["-DKSU_COMPAT_USE_STATIC_KEY=1"])]
        tests += [("policy-lifecycle-init-" + str(i), "test_selinux_policy_lifecycle.c", ["-DINIT_PATH=" + str(i)]) for i in range(3)]
        tests += [("type-creation-vendor-primitives", "test_selinux_type_creation.c", [])]
        for name, fixture, defines in tests:
            binary = output / name
            cmd = common_cc + defines + [str(root / "scripts/susfs" / fixture), "-o", str(binary)]
            (output / (name + "-compile.log")).write_text(run(cmd))
            result = run([str(binary)])
            (output / (name + ".log")).write_text(result)
            print(name + ": " + result.strip())
            results["runs"].append({"name": name, "exit_code": 0, "output": result.strip()})
        negative_binary = output / "baseline-status-negative"
        run(common_cc + ["-DBASELINE_STATUS_NEGATIVE=1", str(root / "scripts/susfs/test_selinux_autohooks.c"), "-o", str(negative_binary)])
        negative_run = subprocess.run([str(negative_binary)], capture_output=True, text=True)
        (output / "baseline-status-negative.log").write_text(negative_run.stdout + negative_run.stderr)
        assert negative_run.returncode and "heap-use-after-free" in negative_run.stderr
        results["baseline_status"] = "expected ASan heap-use-after-free reproduced from actual upstream open callback"
        name = "type-creation-baseline-negative"
        binary = output / name
        baseline_cc = [arg for arg in common_cc if arg != "-D" + MACRO + "=1"]
        run(baseline_cc + ["-DBASELINE_TYPE_CREATION=1", str(root / "scripts/susfs/test_selinux_type_creation.c"), "-o", str(binary)])
        negative_types = run([str(binary)])
        (output / (name + ".log")).write_text(negative_types)
        assert "EXPECTED NEGATIVE" in negative_types
        results["baseline_type_creation"] = negative_types.strip()
        for layout_macro in ["KSU_COMPAT_HAS_MODERN_POLICYDB", "KSU_COMPAT_TYPE_ATTR_MAP_ARRAY_NOT_FOUND"]:
            rejected = subprocess.run(common_cc + ["-D" + layout_macro + "=1", "-fsyntax-only", str(root / "scripts/susfs/test_selinux_type_creation.c")], capture_output=True, text=True)
            assert rejected.returncode and "requires the audited flex-array policydb layout" in rejected.stderr
            (output / (layout_macro + "-rejection.log")).write_text(rejected.stderr)
        results["patched_files"] = {name: digest(work / name) for name in PATCH_FILES}
    inputs = [patch, Path(__file__).resolve(), root / "scripts/susfs/source-pins.json",
              *[root / "scripts/susfs" / name for name in ["test_selinux_autohooks.c", "test_selinux_policy_lifecycle.c", "test_selinux_type_creation.c", "test_selinux_adapter.py"]],
              root / "security/selinux/hooks.c", root / "security/selinux/selinuxfs.c",
              *[root / name for name in ["Makefile", "lib/flex_array.c", "include/linux/flex_array.h",
                "lib/reciprocal_div.c", "include/linux/reciprocal_div.h", "security/selinux/ss/ebitmap.c",
                "security/selinux/ss/ebitmap.h", "security/selinux/ss/hashtab.c", "security/selinux/ss/hashtab.h",
                "security/selinux/ss/symtab.h", "security/selinux/ss/policydb.h", "security/selinux/ss/services.h"]]]
    results["inputs"] = {str(p.relative_to(root)): digest(p) for p in inputs}
    (output / "results.json").write_text(json.dumps(results, indent=2) + "\n")
    print("All source/host checks passed. ARM64 compilation, hook installation and kernel/runtime behavior remain unvalidated.")

if __name__ == "__main__":
    main()
