#!/usr/bin/env python3
"""Native mount allocation/clone/unwind bodies under explicit host models.

ASan/UBSan fixtures model IDA, refcounts, mount locks and VFS references; they
cannot establish native namespace races, propagation safety or boot behavior.
"""
import hashlib
import json
from pathlib import Path
import re
from kernel_baseline import kernel_source
import subprocess
import sys

sys.dont_write_bytecode = True
from test_selinux_adapter import function
from source_guards import without_feature_guards
from test_setresuid_task_state import section


def main():
    root = Path(__file__).resolve().parents[2]
    out = root / '.backport-context/audit/all-features/mount-host'
    out.mkdir(parents=True, exist_ok=True)
    namespace = (root / 'fs/namespace.c').read_text()
    superblock = (root / 'fs/super.c').read_text()
    native = kernel_source(root, 'fs/namespace.c')
    names = ['mnt_alloc_id', 'mnt_free_id', 'mnt_alloc_group_id', 'mnt_release_group_id',
             'alloc_vfsmnt', 'free_vfsmnt', 'vfs_create_mount', 'clone_mnt', '__lookup_mnt']
    # The feature-disabled implementations must retain the native ownership,
    # vendor data callbacks, propagation lists and all error paths exactly.
    restored = without_feature_guards(namespace, ['CONFIG_KSU_SUSFS_SUS_MOUNT'])
    for name in names:
        assert function(restored, name) == function(native, name), name
    assert (root / 'fs/mount.h').read_text() == subprocess.check_output(
        ['git', 'show', 'HEAD:fs/mount.h'], cwd=root).decode(), 'Native mount header must be exact'
    assert section(restored, 'void __init mnt_init(') == section(native, 'void __init mnt_init(')
    storage = re.search(r'struct susfs_mount \{.*?\n\};', namespace, re.S).group(0)
    storage += '\n' + function(namespace, 'susfs_mount_data')
    cache_size = re.search(r'kmem_cache_create\("mnt_cache",\s*(sizeof\(struct \w+\))', namespace).group(1)
    storage = ('#ifdef CONFIG_KSU_SUSFS_SUS_MOUNT\n' + storage +
               '\n_Static_assert(offsetof(struct susfs_mount,mount)==0,"native prefix");\n' +
               '#define TEST_MOUNT_CACHE_SIZE ' + cache_size + '\n#else\n' +
               '#define TEST_MOUNT_CACHE_SIZE sizeof(struct mount)\n#endif\n')
    (out / 'mount_storage.h').write_text(storage)
    owner = re.search(r'struct susfs_mnt_id_owner \{.*?\n\};', namespace, re.S).group(0)
    enabled = owner + '\n' + ''.join(function(namespace, name) for name in
        ['susfs_alloc_mnt_id', 'susfs_put_mnt_id'])
    body = '#ifdef CONFIG_KSU_SUSFS_SUS_MOUNT\n' + enabled + '#endif\n'
    body += ''.join(function(namespace, name) for name in names[:7])
    body += '#ifdef CONFIG_KSU_SUSFS_SUS_MOUNT\n' + function(namespace, 'susfs_clone_mnt_id') + '#endif\n'
    body += function(namespace, 'clone_mnt') + function(namespace, '__lookup_mnt')
    body += '#ifdef CONFIG_KSU_SUSFS_SUS_MOUNT\n' + ''.join(function(namespace, name) for name in
        ['susfs_mnt_hidden', 'susfs_mnt_id', 'susfs_visible_mount_locked', 'susfs_mnt_nonhidden_id',
         'susfs_mnt_parent_id', 'susfs_mnt_visible_root']) + '#endif\n'
    body += function(superblock, 'get_anon_bdev') + function(superblock, 'free_anon_bdev')
    mount = (root / 'fs/susfs_mount.c').read_text()
    body += '#ifdef CONFIG_KSU_SUSFS_SUS_MOUNT\n' + re.sub(r'^#include .*\n', '', mount, flags=re.M) + '#endif\n'
    header = (root / 'include/linux/susfs.h').read_text()
    payload = re.search(r'struct st_susfs_hide_sus_mnts_for_non_su_procs \{.*?\n\};', header, re.S).group(0)
    macros = '\n'.join(re.findall(r'^#define DEFAULT_KSU_MNT_.*', (root / 'include/linux/susfs_def.h').read_text(), re.M))
    (out / 'payload.h').write_text(payload + '\n' + macros + '\n')
    (out / 'native_mount.h').write_text(body)
    inputs = ['fs/namespace.c', 'fs/mount.h', 'fs/susfs_mount.c', 'include/linux/susfs_mount.h',
              'include/linux/susfs.h', 'include/linux/susfs_def.h', 'fs/super.c', 'fs/pnode.h',
              'fs/stat.c', 'fs/statfs.c', 'fs/fhandle.c', 'fs/proc_namespace.c', 'fs/proc/fd.c',
              'fs/notify/fdinfo.c', 'include/uapi/linux/stat.h', 'include/linux/stat.h',
              'scripts/susfs/test_susfs_mount.py', 'scripts/susfs/test_susfs_mount.c',
              'scripts/susfs/source_guards.py']
    report = {'scope': __doc__, 'inputs': {p: hashlib.sha256((root / p).read_bytes()).hexdigest() for p in inputs},
              'native_off_restoration': names + ['mnt_init'], 'native_mount_header_exact': True, 'runs': [], 'negative_controls': [], 'abi': []}
    compiler = ['clang', '-std=gnu11', '-O1', '-g', '-D_GNU_SOURCE', '-Wall', '-Wextra', '-Werror',
                '-Wno-unused-function', '-Wno-unused-parameter', '-Wno-unused-variable',
                '-Wno-sign-compare', '-fsanitize=address,undefined', '-fno-sanitize-recover=undefined',
                '-fno-omit-frame-pointer', '-I', str(out)]
    fixture = root / 'scripts/susfs/test_susfs_mount.c'

    def execute(name, flags, expect_success=True):
        result = subprocess.run(compiler + flags + [str(fixture), '-o', str(out / name)], text=True, capture_output=True)
        (out / (name + '-compile.log')).write_text(result.stdout + result.stderr)
        assert result.returncode == 0, result.stderr
        result = subprocess.run([str(out / name)], text=True, capture_output=True)
        (out / (name + '.log')).write_text(result.stdout + result.stderr)
        assert (result.returncode == 0) == expect_success, result.stdout + result.stderr
        print(name + ': ' + ('passed' if expect_success else 'mutation rejected'))
        return {'name': name, 'exit': result.returncode}

    for on in [False, True]:
        for smp in [False, True]:
            flags = (['-DCONFIG_KSU_SUSFS_SUS_MOUNT=1'] if on else []) + (['-DCONFIG_SMP=1'] if smp else [])
            report['runs'].append(execute('mount-' + str(int(on)) + '-' + str(int(smp)), flags))
    mutations = {
        'view-ref-missing': ('refcount_inc(&owner->refs);', '/* missing view reference */'),
        'view-release-missing': ('susfs_put_mnt_id(susfs_mount_data(mnt)->susfs_view_owner);', '/* leaked view */'),
        'class-lost': ('susfs_mount_data(mnt)->susfs_hidden = hidden;', 'susfs_mount_data(mnt)->susfs_hidden = false;'),
        'lookup-leak': ('susfs_mnt_hide_current() && susfs_mount_data(p)->susfs_hidden', 'false'),
        'path-ref-missing': ('path->mnt = mntget(&visible->mnt);', 'path->mnt = &visible->mnt;'),
    }
    for name, (old, new) in mutations.items():
        assert body.count(old) == 1, name
        (out / 'native_mount.h').write_text(body.replace(old, new))
        report['negative_controls'].append(execute(name, ['-DCONFIG_KSU_SUSFS_SUS_MOUNT=1', '-DCONFIG_SMP=1'], False))
    (out / 'native_mount.h').write_text(body)
    assert storage.count('sizeof(struct susfs_mount)') == 1
    (out / 'mount_storage.h').write_text(storage.replace('sizeof(struct susfs_mount)', 'sizeof(struct mount)'))
    report['negative_controls'].append(execute('cache-too-small', ['-DCONFIG_KSU_SUSFS_SUS_MOUNT=1', '-DCONFIG_SMP=1'], False))
    (out / 'mount_storage.h').write_text(storage)
    # Exact UAPI declarations must keep the original 256-byte statx ABI and
    # the Linux mount-ID offset on both Android userspace ABIs.
    stat = (root / 'include/uapi/linux/stat.h').read_text()
    structs = re.search(r'struct statx_timestamp \{.*?\n\};', stat, re.S).group(0)
    structs += '\n' + re.search(r'struct statx \{.*?\n\};', stat, re.S).group(0)
    for target in ['aarch64-linux-android', 'armv7a-linux-androideabi']:
        src = out / (target + '-abi.c')
        src.write_text('typedef unsigned int __u32; typedef int __s32; typedef unsigned short __u16;\n'
                       'typedef unsigned long long __u64; typedef long long __s64;\n' + structs + '\n'
                       '_Static_assert(sizeof(struct statx)==256,"size");\n'
                       '_Static_assert(__builtin_offsetof(struct statx,stx_mnt_id)==144,"mntid");\n')
        result = subprocess.run(['clang', '--target=' + target, '-ffreestanding', '-nostdinc', '-Werror',
                                 '-fsyntax-only', str(src)], text=True, capture_output=True)
        (out / (target + '-abi.log')).write_text(result.stdout + result.stderr)
        assert result.returncode == 0, result.stderr
        report['abi'].append({'target': target, 'exit': 0})
    (out / 'results.json').write_text(json.dumps(report, indent=2) + '\n')


if __name__ == '__main__':
    main()
