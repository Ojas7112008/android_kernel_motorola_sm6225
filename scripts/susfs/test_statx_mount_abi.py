#!/usr/bin/env python3
"""Actual statx bodies and target kstat layout regression probes.

VFS/LSM/rule stubs model ownership and errors; no native races or boot proof.
Real module CRC comparison remains a separate full-build validation gate.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
from kernel_baseline import kernel_source
import subprocess
import sys

sys.dont_write_bytecode = True
from test_selinux_adapter import function
from test_setresuid_task_state import section


def main():
    root = Path(__file__).resolve().parents[2]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, default=root / '.backport-context/audit/statx-mount-abi')
    args = parser.parse_args()
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    stat = (root / 'fs/stat.c').read_text()
    header = (root / 'include/linux/stat.h').read_text()
    native_header = kernel_source(root, 'include/linux/stat.h')
    assert header == native_header, 'Native kstat header must be restored exactly'
    kstat = re.search(r'struct kstat \{.*?\n\};', header, re.S).group(0)
    uapi = (root / 'include/uapi/linux/stat.h').read_text()
    structs = '\n'.join(re.search(r'struct ' + name + r' \{.*?\n\};', uapi, re.S).group(0)
                        for name in ['statx_timestamp', 'statx'])
    macros = '\n'.join(re.findall(r'^#define STATX_.*', uapi, re.M))
    prefix = ('typedef unsigned int u32, uint32_t, dev_t, __u32; typedef int __s32;\n'
              'typedef unsigned short umode_t, __u16; typedef long long __s64, loff_t;\n'
              'typedef unsigned long long u64, __u64;\n'
              'typedef struct { u32 val; } kuid_t; typedef struct { u32 val; } kgid_t;\n'
              'struct timespec64 { __s64 tv_sec; long tv_nsec; };\n')
    # Avoid libc's dev_t/loff_t typedefs in the host fixture; targets use no libc.
    payload = '#define dev_t model_dev_t\n#define loff_t model_loff_t\n' + prefix + macros + '\n' + kstat + '\n' + structs + '\n'
    (out / 'stat_payload.h').write_text(payload)
    lookup = (root / 'include/linux/namei.h').read_text()
    (out / 'lookup_flags.h').write_text('\n'.join(re.findall(r'^#define\s+LOOKUP_\w+\s+[^\n]+', lookup, re.M)) + '\n')
    public = ['vfs_getattr_nosec', 'vfs_getattr', 'vfs_statx']
    original = kernel_source(root, 'fs/stat.c')
    for name in public:
        assert function(stat, name).split('{', 1)[0] == function(original, name).split('{', 1)[0], name
        assert 'EXPORT_SYMBOL(' + name + ');' in stat
    bodies = ''.join(function(stat, name) for name in
        ['vfs_getattr_nosec_mnt_id', 'vfs_getattr_nosec', 'vfs_getattr_mnt_id', 'vfs_getattr',
         'vfs_statx_mnt_id', 'vfs_statx'])
    bodies += section(stat, 'static noinline_for_stack int\ncp_statx(')
    bodies += section(stat, 'SYSCALL_DEFINE5(statx,')
    (out / 'native_statx.h').write_text(bodies)
    inputs = ['fs/stat.c', 'fs/susfs_kstat.c', 'include/linux/stat.h', 'include/uapi/linux/stat.h',
              'include/linux/susfs_meta.h', 'scripts/susfs/test_statx_mount_abi.py',
              'scripts/susfs/test_statx_mount_abi.c']
    report = {'scope': __doc__, 'inputs': {n: hashlib.sha256((root / n).read_bytes()).hexdigest() for n in inputs},
              'native_kstat_header_exact': True, 'exported_prototypes_unchanged': public,
              'runs': [], 'abi': [], 'negative_controls': []}
    cc = ['clang', '-std=gnu11', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
          '-Wno-unused-function', '-Wno-unused-parameter', '-Wno-sign-compare',
          '-fsanitize=address,undefined', '-fno-sanitize-recover=undefined',
          '-fno-omit-frame-pointer', '-I', str(out)]
    fixture = root / 'scripts/susfs/test_statx_mount_abi.c'

    def execute(name, flags, expect_success=True):
        compiled = subprocess.run(cc + flags + [str(fixture), '-o', str(out / name)], capture_output=True, text=True)
        (out / (name + '-compile.log')).write_text(compiled.stdout + compiled.stderr)
        assert compiled.returncode == 0, compiled.stderr
        run = subprocess.run([str(out / name)], capture_output=True, text=True)
        (out / (name + '.log')).write_text(run.stdout + run.stderr)
        assert (run.returncode == 0) == expect_success, run.stdout + run.stderr
        print(name + ': ' + ('passed' if expect_success else 'mutation rejected'))
        return {'name': name, 'exit': run.returncode}

    modes = [('native', [])] + [('mount-' + str(mount) + '-kstat-' + str(kstat),
        ['-DCONFIG_KSU_SUSFS=1'] + (['-DCONFIG_KSU_SUSFS_SUS_MOUNT=1'] if mount else []) +
        (['-DCONFIG_KSU_SUSFS_SUS_KSTAT=1'] if kstat else [])) for mount in [0, 1] for kstat in [0, 1]]
    for name, flags in modes:
        report['runs'].append(execute(name, flags))
    mutations = {
        'mntid-lost': ('tmp.stx_mnt_id = mnt_id;', 'tmp.stx_mnt_id = 0;'),
        'transport-lost': ('&stat, mask, &mnt_id);', '&stat, mask, NULL);'),
        'lsm-bypassed': ('retval = security_inode_getattr(path);', 'retval = 0;'),
        'path-released-early': ('error = vfs_getattr_mnt_id(&path, stat, request_mask, flags, mnt_id);\n\tpath_put(&path);',
                                'path_put(&path);\n\terror = vfs_getattr_mnt_id(&path, stat, request_mask, flags, mnt_id);'),
    }
    for name, (old, new) in mutations.items():
        assert bodies.count(old) == 1, name
        (out / 'native_statx.h').write_text(bodies.replace(old, new))
        report['negative_controls'].append(execute(name, modes[-1][1], False))
    (out / 'native_statx.h').write_text(bodies)
    baseline = kstat.replace('struct kstat {', 'struct native_kstat {', 1)
    fields = re.findall(r'^\s*(?:struct\s+\w+|[a-zA-Z0-9_]+)\s+(\w+)\s*;', kstat, re.M)
    checks = ('_Static_assert(sizeof(struct kstat)==sizeof(struct native_kstat),"native size");\n'
              '_Static_assert(_Alignof(struct kstat)==_Alignof(struct native_kstat),"native alignment");\n')
    checks += ''.join('_Static_assert(__builtin_offsetof(struct kstat,' + field +
                      ')==__builtin_offsetof(struct native_kstat,' + field + '),"' + field + '");\n' for field in fields)
    checks += ('_Static_assert(sizeof(struct statx)==256,"statx size");\n'
               '_Static_assert(__builtin_offsetof(struct statx,stx_mnt_id)==144,"statx mount offset");\n')
    bad = kstat.replace('\n};', '\n#ifdef CONFIG_KSU_SUSFS_SUS_MOUNT\n u64 mnt_id;\n#endif\n};')
    for target in ['aarch64-linux-android', 'armv7a-linux-androideabi']:
        for enabled in [0, 1]:
            name = target + '-mount-' + str(enabled)
            unit = out / (name + '.c')
            unit.write_text(prefix + macros + '\n' + kstat + '\n' + baseline + '\n' + structs + '\n' + checks)
            command = ['clang', '--target=' + target, '-ffreestanding', '-nostdinc', '-std=gnu11', '-Werror',
                       '-DCONFIG_KSU_SUSFS_SUS_MOUNT=' + str(enabled)] if enabled else [
                       'clang', '--target=' + target, '-ffreestanding', '-nostdinc', '-std=gnu11', '-Werror']
            r = subprocess.run(command + ['-fsyntax-only', str(unit)], capture_output=True, text=True)
            (out / (name + '.log')).write_text(r.stdout + r.stderr)
            assert r.returncode == 0, r.stderr
            report['abi'].append({'target': target, 'mount': enabled, 'field_offsets': len(fields), 'exit': 0})
        unit = out / (target + '-old-broken-layout.c')
        unit.write_text(prefix + macros + '\n' + bad + '\n' + baseline + '\n' + structs + '\n' + checks)
        r = subprocess.run(['clang', '--target=' + target, '-ffreestanding', '-nostdinc', '-std=gnu11',
                            '-DCONFIG_KSU_SUSFS_SUS_MOUNT=1', '-fsyntax-only', str(unit)], capture_output=True, text=True)
        (out / (target + '-old-broken-layout.log')).write_text(r.stdout + r.stderr)
        assert r.returncode and 'native size' in r.stderr
        report['negative_controls'].append({'name': target + '-old-layout', 'exit': r.returncode})
    (out / 'results.json').write_text(json.dumps(report, indent=2) + '\n')
    print('Native kstat layout and statx UAPI preserved on ARM64/ARM32; old layout rejected.')


if __name__ == '__main__':
    main()
