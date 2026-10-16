#!/usr/bin/env python3
"""Actual kstat rule/consumer bodies with host VFS/RCU models and target ABI probes.

Models do not prove native RCU races, filesystem lifetime or device behavior.
"""
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys

sys.dont_write_bytecode = True
from test_selinux_adapter import function
from test_setresuid_task_state import section


def main():
    root = Path(__file__).resolve().parents[2]
    out = root / '.backport-context/audit/all-features/kstat-host'
    out.mkdir(parents=True, exist_ok=True)
    inc = out / 'linux'
    inc.mkdir(exist_ok=True)
    for name in ['compat.h', 'fs.h', 'hashtable.h', 'kdev_t.h', 'mutex.h', 'namei.h',
                 'rcupdate.h', 'slab.h', 'stat.h', 'string.h', 'uaccess.h', 'types.h']:
        (inc / name).write_text('/* Explicit fixture model. */\n')
    (inc / 'errno.h').write_text('#include_next <linux/errno.h>\n')
    (inc / 'overflow.h').write_text('#define check_add_overflow(a,b,p) __builtin_add_overflow(a,b,p)\n')
    (inc / 'susfs_mount.h').write_text('/* Explicit fixture model; native helpers tested separately. */\n')
    (out / 'mount.h').write_text('/* Native mount model defined by fixture. */\n')
    header = (root / 'include/linux/susfs.h').read_text()
    defs = (root / 'include/linux/susfs_def.h').read_text()
    public = re.search(r'struct st_susfs_sus_kstat \{.*?\n\};', header, re.S).group(0)
    macros = '\n'.join(re.findall(r'^#define KSTAT_SPOOF_.*', header, re.M))
    (inc / 'susfs.h').write_text('#pragma once\n#define SUSFS_MAX_LEN_PATHNAME 256\n' + macros + '\n' + public + '\n')
    (inc / 'susfs_meta.h').write_bytes((root / 'include/linux/susfs_meta.h').read_bytes())
    (out / 'kstat_handler.h').write_bytes((root / 'fs/susfs_kstat.c').read_bytes())
    stat = (root / 'fs/stat.c').read_text()
    statfs = (root / 'fs/statfs.c').read_text()
    (out / 'native_getattr.h').write_text(''.join(function(stat, name) for name in
        ['vfs_getattr_nosec_mnt_id', 'vfs_getattr_nosec', 'vfs_getattr_mnt_id', 'vfs_getattr']))
    (out / 'native_statfs.h').write_text(function(statfs, 'flags_by_mnt') + function(statfs, 'flags_by_sb') +
        function(statfs, 'calculate_f_flags') + function(statfs, 'statfs_by_dentry') + function(statfs, 'vfs_statfs') +
        '#ifdef CONFIG_KSU_SUSFS_SUS_KSTAT\n' + function(statfs, 'susfs_statfs_snapshot') + '#endif\n')
    kdev = (root / 'include/linux/kdev_t.h').read_text()
    (out / 'native_dev.h').write_text(section(kdev, 'static __always_inline dev_t new_decode_dev(') +
                                     section(kdev, 'static __always_inline dev_t huge_decode_dev('))
    inputs = ['fs/susfs_kstat.c', 'include/linux/susfs_meta.h', 'include/linux/susfs.h', 'include/linux/susfs_def.h',
              'include/linux/kdev_t.h', 'fs/stat.c', 'fs/statfs.c', 'fs/proc/task_mmu.c', 'fs/notify/fdinfo.c',
              'fs/proc/fd.c', 'scripts/susfs/test_susfs_kstat.py', 'scripts/susfs/test_susfs_kstat.c']
    report = {'scope': __doc__, 'inputs': {name: hashlib.sha256((root / name).read_bytes()).hexdigest() for name in inputs}, 'runs': []}
    cc = ['clang', '-std=gnu11', '-O1', '-g', '-D_GNU_SOURCE', '-Wall', '-Wextra', '-Werror',
          '-Wno-unused-function', '-Wno-unused-parameter', '-Wno-unused-variable', '-Wno-sign-compare',
          '-fsanitize=address,undefined', '-fno-sanitize-recover=undefined', '-fno-omit-frame-pointer', '-I', str(out)]
    for enabled, mount in [(False, False), (True, False), (False, True), (True, True)]:
        name = 'kstat-' + str(int(enabled)) + '-mount-' + str(int(mount))
        command = cc + (['-DCONFIG_KSU_SUSFS_SUS_KSTAT=1'] if enabled else []) + (['-DCONFIG_KSU_SUSFS_SUS_MOUNT=1'] if mount else [])
        command += [str(root / 'scripts/susfs/test_susfs_kstat.c'), '-o', str(out / name)]
        result = subprocess.run(command, text=True, capture_output=True)
        (out / (name + '-compile.log')).write_text(result.stdout + result.stderr)
        assert result.returncode == 0, result.stderr
        result = subprocess.run([str(out / name)], text=True, capture_output=True)
        (out / (name + '.log')).write_text(result.stdout + result.stderr)
        assert result.returncode == 0, result.stdout + result.stderr
        report['runs'].append({'name': name, 'exit': 0, 'result': result.stdout.strip()})
        print(name + ': ' + result.stdout.strip())
    # Compile the actual payload declarations against the pinned JNI layout.
    user = (root / '.backport-context/repos/susfs/ksu_susfs/jni/features/sus_kstat.c').read_text()
    user = re.search(r'struct st_susfs_sus_kstat \{.*?\n\};', user, re.S).group(0).replace('struct st_susfs_sus_kstat', 'struct userspace_kstat')
    compat = re.search(r'struct susfs_kstat_compat \{.*?\n\};', (root / 'fs/susfs_kstat.c').read_text(), re.S).group(0)
    prefix = '#define bool _Bool\n#define SUSFS_MAX_LEN_PATHNAME 256\n#define __aligned(n) __attribute__((aligned(n)))\ntypedef unsigned char u8; typedef unsigned int u32; typedef int s32; typedef long long s64;\n'
    fields = re.findall(r'^\s*(?:bool|char|unsigned long|unsigned int|long long|long|int)\s+(\w+)(?:\[[^]]+\])?;', public, re.M)
    report['abi'] = []
    for target in ['aarch64-linux-android', 'armv7a-linux-androideabi']:
        assertions = '_Static_assert(sizeof(struct st_susfs_sus_kstat)==sizeof(struct userspace_kstat), "JNI sizeof");\n'
        assertions += ''.join('_Static_assert(__builtin_offsetof(struct st_susfs_sus_kstat,' + f + ')==__builtin_offsetof(struct userspace_kstat,' + f + '), "JNI ' + f + '");\n' for f in fields)
        if target.startswith('armv7'):
            assertions += '_Static_assert(sizeof(struct susfs_kstat_compat)==sizeof(struct userspace_kstat), "compat sizeof");\n'
            assertions += ''.join('_Static_assert(__builtin_offsetof(struct susfs_kstat_compat,' + f + ')==__builtin_offsetof(struct userspace_kstat,' + f + '), "compat ' + f + '");\n' for f in fields)
        source = out / (target + '-abi.c')
        source.write_text(prefix + public + '\n' + user + '\n' + compat + '\n' + assertions)
        result = subprocess.run(['clang', '--target=' + target, '-ffreestanding', '-nostdinc', '-std=gnu11', '-Werror', '-fsyntax-only', str(source)], text=True, capture_output=True)
        (out / (target + '-abi.log')).write_text(result.stdout + result.stderr)
        assert result.returncode == 0, result.stderr
        report['abi'].append({'target': target, 'fields': len(fields), 'exit': 0})
    (out / 'results.json').write_text(json.dumps(report, indent=2) + '\n')


if __name__ == '__main__':
    main()
