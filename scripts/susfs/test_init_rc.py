#!/usr/bin/env python3
"""Extracted init.rc and native read/fstat consumers; host mocks, not a kernel build."""
import json
from pathlib import Path
import re
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
    out = root / ".backport-context/audit/init-rc"
    out.mkdir(parents=True, exist_ok=True)
    patch = root / ".github/patches/bakasu-devon-selinux-policy.patch"
    fixture = root / "scripts/susfs/test_init_rc.c"
    results = {"pin": PIN, "scope": __doc__, "compiler": run(["clang", "--version"]).splitlines()[0], "runs": []}
    with tempfile.TemporaryDirectory(prefix="devon-initrc-") as directory:
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
        source = (work / "kernel/runtime/ksud_integration.c").read_text()
        constant = source[source.index("static const char KERNEL_SU_RC[]"):source.index("// clang-format on")]
        begin = source.index("/* Devon uses held-file inline hooks;")
        end = source.index("\n#else\nstatic ssize_t (*orig_read)", begin)
        target = source[begin:end]
        assert "file->f_op =" not in target and "private_data" not in target.split("\n", 1)[1]
        (out / "provider_init_rc.h").write_text(constant + target + function(source, "is_init_rc"))
        rw = (root / "fs/read_write.c").read_text()
        native = function(rw, "new_sync_read") + function(rw, "__vfs_read") + function(rw, "vfs_read")
        native += function(rw, "do_iter_readv_writev") + function(rw, "do_loop_readv_writev")
        native += function(rw, "do_iter_read") + function(rw, "vfs_iocb_iter_read")
        (out / "native_reads.h").write_text(native)
        (out / "native_fstat_seek.h").write_text(function((root / "fs/stat.c").read_text(), "vfs_statx_fd") + section(rw, "loff_t vfs_llseek("))
        close = function((root / "fs/file_table.c").read_text(), "__fput")
        assert close.index("eventpoll_release(file)") < close.index("ksu_handle_initrc_file_release(file)") < close.index("file->f_op->release(inode, file)") < close.index("\n\tfile_free(file);")
        (out / "native-final-close.h").write_text(close)
        include = out / "linux"
        include.mkdir(exist_ok=True)
        (include / "types.h").write_text("#ifndef INIT_RC_TYPES\n#define INIT_RC_TYPES\n#include_next <linux/types.h>\n#include <stdint.h>\n#include <sys/types.h>\n#ifndef __user\n#define __user\n#endif\ntypedef uint32_t u32;\n#endif\n")
        (include / "bits.h").write_text("#ifndef BIT\n#define BIT(n) (1UL<<(n))\n#endif\n")
        (include / "string.h").write_text("#include <string.h>\n")
        for name in ["cred.h", "sched.h", "thread_info.h", "uidgid.h"]:
            (include / name).write_text("/* Host declarations precede project headers. */\n")
        for name in ["susfs_def.h", "ksu_susfs.h"]:
            (include / name).write_bytes((root / "include/linux" / name).read_bytes())
        cc = ["clang", "-std=gnu11", "-O1", "-g", "-pthread", "-Wall", "-Wextra", "-Werror",
              "-Wno-unused-parameter", "-Wno-unused-function", "-Wno-unused-variable", "-Wno-sign-compare",
              "-Werror=incompatible-pointer-types", "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
              "-I", str(out)]
        enabled = ["-DCONFIG_KSU_SUSFS=1", "-DCONFIG_KSU=1", "-DCONFIG_KSU_DEVON_SELINUX_POLICY=1",
                   "-DCONFIG_ARM64=1", "-DCONFIG_64BIT=1", "-DCONFIG_THREAD_INFO_IN_TASK=1", "-D" + MACRO + "=1"]
        for name, flags in [("hooks-off", []), ("manual-native-path", ["-DCONFIG_KSU_MANUAL_HOOK=1"]),
                            ("susfs-static-key-0", enabled), ("susfs-static-key-1", enabled + ["-DKSU_COMPAT_USE_STATIC_KEY=1"])]:
            binary = out / name
            compile_result = subprocess.run(cc + flags + [str(fixture), "-o", str(binary)], capture_output=True, text=True)
            (out / (name + "-compile.log")).write_text(compile_result.stdout + compile_result.stderr)
            assert not compile_result.returncode, compile_result.stderr
            result = subprocess.run([str(binary)], capture_output=True, text=True)
            (out / (name + ".log")).write_text(result.stdout + result.stderr)
            assert not result.returncode, result.stdout + result.stderr
            results["runs"].append({"name": name, "exit": 0, "result": result.stdout.strip()})
            print(name + ": " + result.stdout.strip())
        # Actual pinned proxy/fstat as a semantic negative control. Module
        # discovery is mocked to isolate its cursor/copy/stat lifetime behavior.
        pinned = (ref / "kernel/runtime/ksud_integration.c").read_text()
        prefix = fixture.read_text().split("#ifdef CONFIG_KSU_SUSFS\nbool ksu_susfs_cred_ready")[0]
        prefix = re.sub(r"^static ssize_t vfs_read[^\n]*\n", "", prefix, flags=re.M)
        prefix = re.sub(r"^static ssize_t ksu_kernel_read_compat[^\n]*\n", "", prefix, flags=re.M)
        old = constant + "static ssize_t (*orig_read)(struct file *,char *,size_t,loff_t *);\n"
        old += "static ssize_t (*orig_read_iter)(struct kiocb *,struct iov_iter *);\n"
        old += "static ssize_t ksu_rc_pos,module_rc_pos;\nconst size_t ksu_rc_len=sizeof(KERNEL_SU_RC)-1;\n"
        old += "static char *module_rc_buf;\nstatic size_t module_rc_len;\n"
        old += "static void load_module_rc_once(void) {static bool loaded;if(loaded)return;loaded=true;module_rc_len=sizeof(module_data)-1;module_rc_buf=vmalloc(module_rc_len);assert(module_rc_buf);memcpy(module_rc_buf,module_data,module_rc_len);}\n"
        old += "".join(function(pinned, name) for name in ["free_module_rc", "read_proxy", "read_iter_proxy", "is_init_rc", "ksu_handle_vfs_fstat"])
        negative_main = r"""
int main(void) {
    inodes[0]=(struct inode){S_IFREG,sizeof(base_data)-1,&sb};
    dentries[0]=(struct dentry){&inodes[0],{"init.rc"}};
    rc[0]=(struct file){.f_path={&dentries[0],"/init.rc"},.f_mode=FMODE_READ|FMODE_CAN_READ|FMODE_OPENED,.f_op=&read_ops,.data=base_data,.length=sizeof(base_data)-1,.refs=1};
    fd_file=&rc[0];subjective=&other_cred;task.pid=2;
    loff_t first_size=rc[0].length;
    ksu_handle_vfs_fstat(7,&first_size);
    assert(first_size==(loff_t)(rc[0].length+ksu_rc_len+module_rc_len));
    orig_read=backend_read;orig_read_iter=backend_iter;
    char buf[4096];loff_t pos=rc[0].length;copy_budget=0;
    assert(!read_proxy(&rc[0],buf,sizeof(buf),&pos));
    copy_budget=SIZE_MAX;
    assert(read_proxy(&rc[0],buf,sizeof(buf),&pos)==first_size-(loff_t)rc[0].length);
    assert(pos==(loff_t)rc[0].length && !module_rc_len && !module_rc_buf);
    loff_t second_size=rc[0].length;ksu_handle_vfs_fstat(7,&second_size);
    assert(second_size<first_size && second_size==(loff_t)(rc[0].length+ksu_rc_len));
    assert(!live_allocations && rc[0].refs==1);
    puts("EXPECTED NEGATIVE: pinned proxy reports false EOF on copy failure, leaves logical position unchanged, drops module fstat size after read, and trusts spoofed init comm for fstat");
    return 0;
}
"""
        unit = out / "pinned-init-rc-negative.c"
        unit.write_text(prefix + old + negative_main)
        binary = out / "pinned-init-rc-negative"
        compiled = subprocess.run(cc + enabled + [str(unit), "-o", str(binary)], capture_output=True, text=True)
        (out / "pinned-init-rc-negative-compile.log").write_text(compiled.stderr)
        assert not compiled.returncode, compiled.stderr
        control = subprocess.run([str(binary)], capture_output=True, text=True)
        (out / "pinned-init-rc-negative.log").write_text(control.stdout + control.stderr)
        assert not control.returncode and "EXPECTED NEGATIVE" in control.stdout, control.stdout + control.stderr
        results["negative_control"] = control.stdout.strip()
        print(control.stdout.strip())
        results["provider_files"] = {name: digest(work / name) for name in PATCH_FILES}
    inputs = [patch, fixture, Path(__file__).resolve(), root / "scripts/susfs/test_selinux_autohooks.py",
              root / "scripts/susfs/test_selinux_adapter.py", root / "scripts/susfs/test_setresuid_task_state.py",
              root / "include/linux/ksu_susfs.h", root / "include/linux/susfs_def.h", root / "fs/read_write.c",
              root / "fs/stat.c", root / "fs/file_table.c"]
    results["inputs"] = {str(p.relative_to(root)): digest(p) for p in inputs}
    (out / "results.json").write_text(json.dumps(results, indent=2) + "\n")
    print("Host/source checks pass; native file lifetime, VFS/LSM, ARM64 compilation and device behavior remain unvalidated.")

if __name__ == "__main__":
    main()
