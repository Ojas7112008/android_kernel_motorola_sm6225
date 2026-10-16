#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0
"""Exercise the actual exec wrapper with consuming-filename mocks and sanitizers.

This host-side test checks wrapper ownership only. It does not compile the
kernel, validate BakaSU itself, or establish ARM64/boot/runtime compatibility.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
from kernel_baseline import kernel_source
import subprocess
import tempfile

BASELINE = "18bd9752ada79090ce11326662eaaa9f33717728"

PRELUDE = r"""
#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct filename { int refcnt; char *name; };
struct user_arg_ptr { void *ptr; };
#define IS_ERR(p) ((uintptr_t)(p) >= (uintptr_t)-4095)
#define IS_ERR_OR_NULL(p) (!(p) || IS_ERR(p))
#define PTR_ERR(p) ((int)(intptr_t)(p))
#define ERR_PTR(err) ((struct filename *)(intptr_t)(err))

static int scenario, created, freed, post_calls, last_seen;
static struct filename *audit_reference;

static struct filename *new_filename(const char *name)
{
    struct filename *p = calloc(1, sizeof(*p));
    assert(p);
    p->refcnt = 1;
    p->name = strdup(name);
    assert(p->name);
    created++;
    return p;
}

static void putname(struct filename *p)
{
    assert(!IS_ERR_OR_NULL(p));
    assert(p->refcnt > 0);
    if (--p->refcnt)
        return;
    free(p->name);
    free(p);
    freed++;
}

int ksu_handle_execveat(int *fd, struct filename **p, void *argv,
                       void *envp, int *flags)
{
    (void)fd; (void)argv; (void)envp; (void)flags;
    if (scenario == 5 && !IS_ERR_OR_NULL(*p)) {
        putname(*p);
        *p = new_filename("replacement");
    } else if (scenario == 6 && !IS_ERR_OR_NULL(*p)) {
        putname(*p);
        *p = ERR_PTR(-ENOENT);
    }
    return 0;
}

int ksu_handle_post_execveat(int *fd, struct filename **p, void *argv,
                            void *envp, int *flags, int *retval)
{
    (void)fd; (void)argv; (void)envp; (void)flags; (void)retval;
    if (IS_ERR_OR_NULL(*p))
        return -EINVAL;
    /* Model the pinned BakaSU wrapper's filename->name dereference. */
    last_seen = (unsigned char)(*p)->name[0];
    assert((*p)->refcnt > 0);
    post_calls++;
    /* The retained reference must not depend on a mutated hook argument. */
    if (scenario == 7)
        *p = NULL;
    return 0;
}

static int __do_execve_file(int fd, struct filename *p,
                            struct user_arg_ptr argv,
                            struct user_arg_ptr envp, int flags, void *file)
{
    (void)fd; (void)argv; (void)envp; (void)flags; (void)file;
    if (IS_ERR(p))
        return PTR_ERR(p);
    if (!p)
        return -EINVAL;
    putname(p); /* Both real success and error cleanup consume this ref. */
    switch (scenario) {
    case 1: return -ENOMEM;
    case 2: return -EAGAIN;
    case 3: return -ENOENT;
    default: return 0;
    }
}
"""

POSTLUDE = r"""
int main(int argc, char **argv)
{
    assert(argc == 2);
    scenario = atoi(argv[1]);
    assert(scenario >= 0 && scenario <= 8);
    struct filename *p = scenario == 4 ? ERR_PTR(-EFAULT)
                                       : new_filename("initial");
    if (scenario == 8) {
        audit_reference = p;
        p->refcnt++;
    }
    struct user_arg_ptr args = { 0 };
    int ret = do_execveat_common(-100, p, args, args, 0);
    int expected = 0;
    if (scenario == 1) expected = -ENOMEM;
    if (scenario == 2) expected = -EAGAIN;
    if (scenario == 3) expected = -ENOENT;
    if (scenario == 4) expected = -EFAULT;
#if defined(CONFIG_KSU_MANUAL_HOOK) || defined(CONFIG_KSU_SUSFS)
    if (scenario == 6) expected = -ENOENT;
    if (scenario == 4 || scenario == 6) {
        assert(post_calls == 0);
    } else {
        assert(post_calls == 1);
        assert(last_seen == (scenario == 5 ? 'r' : 'i'));
    }
#else
    assert(post_calls == 0);
#endif
    assert(ret == expected);
    if (audit_reference) {
        assert(audit_reference->refcnt == 1);
        putname(audit_reference);
    }
    assert(created == freed);
    puts("PASS");
    return 0;
}
"""


def wrapper(source):
    start = source.index("static int do_execveat_common(")
    opening = source.index("{", start)
    depth = 0
    for index in range(opening, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[start:index + 1]
    raise ValueError("Unterminated exec wrapper")


def run(command, **kwargs):
    return subprocess.run(command, text=True, capture_output=True,
                          timeout=30, **kwargs)


def main():
    root = Path(__file__).resolve().parents[2]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", default="clang")
    parser.add_argument("--baseline-ref", default=BASELINE)
    parser.add_argument("--log-dir", type=Path,
                        default=root / ".backport-context/audit/exec-lifetime")
    args = parser.parse_args()
    args.log_dir.mkdir(parents=True, exist_ok=True)
    current = (root / "fs/exec.c").read_text()
    baseline = (subprocess.CompletedProcess([], 0, kernel_source(root, 'fs/exec.c', legacy=True), '')
                if args.baseline_ref == BASELINE else
                run(["git", "show", args.baseline_ref + ":fs/exec.c"], cwd=root))
    if baseline.returncode:
        raise RuntimeError(baseline.stderr)
    compiler = run([args.compiler, "--version"])
    if compiler.returncode:
        raise RuntimeError(compiler.stderr)
    flags = ["-std=gnu11", "-O1", "-g", "-Wall", "-Wextra", "-Werror",
             "-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
    environment = dict(os.environ, ASAN_OPTIONS="detect_leaks=1:abort_on_error=1",
                       UBSAN_OPTIONS="halt_on_error=1")
    report = {
        "scope": "Host wrapper ownership with mocks; not a kernel/BakaSU build",
        "compiler": compiler.stdout.splitlines()[0],
        "baseline_ref": args.baseline_ref,
        "source_sha256": hashlib.sha256(current.encode()).hexdigest(),
        "cases": [],
    }
    with tempfile.TemporaryDirectory(prefix="harness-", dir=args.log_dir) as tmp:
        tmp = Path(tmp)
        for label, source, hook in [
            ("baseline-manual", baseline.stdout, "CONFIG_KSU_MANUAL_HOOK"),
            ("fixed-manual", current, "CONFIG_KSU_MANUAL_HOOK"),
            ("fixed-susfs", current, "CONFIG_KSU_SUSFS"),
            ("fixed-no-hook", current, None),
        ]:
            unit, executable = tmp / (label + ".c"), tmp / label
            unit.write_text(PRELUDE + "\n" + wrapper(source) + "\n" + POSTLUDE)
            command = [args.compiler, *flags]
            if hook:
                command.append("-D" + hook + "=1")
            command += [str(unit), "-o", str(executable)]
            compiled = run(command)
            (args.log_dir / (label + "-compile.log")).write_text(
                "Command: " + " ".join(command) + "\nExit: " +
                str(compiled.returncode) + "\n" + compiled.stdout + compiled.stderr)
            if compiled.returncode:
                raise RuntimeError(compiled.stderr)
            for case in ([0] if label.startswith("baseline") else range(9)):
                result = run([str(executable), str(case)], env=environment)
                (args.log_dir / (label + "-" + str(case) + ".log")).write_text(
                    "Exit: " + str(result.returncode) + "\n" +
                    result.stdout + result.stderr)
                if label.startswith("baseline"):
                    passed = (result.returncode != 0 and
                              "heap-use-after-free" in result.stderr)
                    interpretation = "Expected ASan failure reproduces old ownership bug"
                else:
                    passed = result.returncode == 0 and result.stdout.strip() == "PASS"
                    interpretation = "Ownership, return code and reference balance passed"
                report["cases"].append({
                    "variant": label, "case": case, "exit_code": result.returncode,
                    "passed": passed, "interpretation": interpretation,
                })
                if not passed:
                    raise AssertionError(label + "/" + str(case) + "\n" + result.stderr)
    report["passed"] = True
    (args.log_dir / "results.json").write_text(json.dumps(report, indent=2) + "\n")
    print("Baseline: expected heap-use-after-free reproduced")
    print("Fixed wrapper: 9 manual-hook, 9 SUSFS-hook and 9 no-hook cases passed with ASan/UBSan")
    print("Host test only; no kernel build or device validation")


if __name__ == "__main__":
    main()
