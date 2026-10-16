#!/usr/bin/env python3
"""Native overlay helper bodies and SUSFS identity policy under sequential host models.

No overlay mount, daemon, native RCU/copy-up concurrency or kernel module is run.
This checkpoint does not enable production SUS_PATH.
"""
import json
from pathlib import Path
import re
from kernel_baseline import kernel_source
import subprocess
import sys
sys.dont_write_bytecode = True
from test_selinux_adapter import function, run, digest
from test_setresuid_task_state import section


def main():
    root = Path(__file__).resolve().parents[2]
    out = root / '.backport-context/audit/path-open-create/overlay'
    out.mkdir(parents=True, exist_ok=True)
    sources = {name: (root / name).read_text() for name in [
        'include/linux/dcache.h', 'fs/overlayfs/ovl_entry.h', 'fs/overlayfs/overlayfs.h',
        'fs/overlayfs/util.c', 'fs/overlayfs/super.c', 'fs/overlayfs/inode.c', 'fs/inode.c', 'include/linux/fs.h']}
    for name, source in sources.items():
        assert source.encode() == kernel_source(root, name).encode()
    groups = [
        ('include/linux/dcache.h', ['d_inode', 'd_backing_inode', 'd_real', 'd_real_inode']),
        ('fs/overlayfs/ovl_entry.h', ['OVL_I', 'ovl_upperdentry_dereference']),
        ('fs/overlayfs/overlayfs.h', ['ovl_copyattr', 'ovl_copyflags']),
        ('fs/overlayfs/util.c', ['ovl_dentry_upper', 'ovl_dentry_lowerdata', 'ovl_i_dentry_upper',
          'ovl_inode_upper', 'ovl_inode_lower', 'ovl_inode_real', 'ovl_inode_lowerdata',
          'ovl_test_flag', 'ovl_should_check_upperdata', 'ovl_has_upperdata', 'ovl_inode_realdata',
          'ovl_inode_init', 'ovl_inode_update']),
        ('fs/overlayfs/super.c', ['ovl_d_real']),
    ]
    code = 'static struct dentry *ovl_d_real(struct dentry *, const struct inode *);\n'
    for file, names in groups:
        code += ''.join(function(sources[file], name) for name in names)
    (out / 'native_overlay_helpers.h').write_text(code)
    defs = (root / 'include/linux/susfs_def.h').read_text()
    constants = ''
    for source, names in [(sources['include/linux/fs.h'], ['S_SYNC', 'S_IMMUTABLE', 'S_APPEND', 'S_NOATIME']),
                          (sources['include/linux/dcache.h'], ['DCACHE_OP_REAL']),
                          (defs, ['AS_FLAGS_SUS_PATH', 'TIF_PROC_UMOUNTED'])]:
        constants += ''.join(re.search(r'^#define\s+' + name + r'\s+[^\n]+', source, re.M).group(0) + '\n' for name in names)
    constants += re.search(r'enum ovl_inode_flag\s*\{.*?\};', sources['fs/overlayfs/overlayfs.h'], re.S).group(0) + '\n'
    (out / 'native_overlay_constants.h').write_text(constants)
    predicate = section(defs, 'static inline bool susfs_is_current_proc_umounted_app(')
    predicate += function((root / 'fs/susfs_path.c').read_text(), 'susfs_is_inode_sus_path')
    (out / 'actual_path_predicate.h').write_text(predicate)
    initializer = function(sources['fs/inode.c'], 'inode_init_always')
    assert re.search(r'struct address_space\s*\*const mapping\s*=\s*&inode->i_data;', initializer)
    assert re.search(r'inode->i_mapping\s*=\s*mapping;', initializer)
    for name in ('ovl_inode_init', 'ovl_inode_update'):
        assert 'i_mapping' not in function(sources['fs/overlayfs/util.c'], name)
    fixture = root / 'scripts/susfs/test_path_overlay.c'
    cc = ['clang', '-std=gnu11', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
          '-Wno-unused-function', '-Wno-unused-parameter', '-fsanitize=address,undefined',
          '-fno-sanitize-recover=undefined', '-fno-omit-frame-pointer', '-I', str(out)]
    results = {'scope': __doc__, 'compiler': run(['clang', '--version']).splitlines()[0], 'runs': []}
    for enabled in (False, True):
        name = f'overlay-path-{int(enabled)}'
        flags = ['-DCONFIG_KSU_SUSFS_SUS_PATH=1'] if enabled else []
        (out / (name + '-compile.log')).write_text(run(cc + flags + [str(fixture), '-o', str(out / name)]))
        result = run([str(out / name)])
        (out / (name + '.log')).write_text(result)
        results['runs'].append({'name': name, 'exit': 0, 'result': result.strip()})
        print(name + ': ' + result.strip())
    # Reject replacing the policy's overlay inode with its physical data inode.
    mutant = fixture.read_text().replace('susfs_is_inode_sus_path(d_backing_inode(d))',
                                       'susfs_is_inode_sus_path(d_real_inode(d))')
    assert mutant != fixture.read_text()
    (out / 'wrong-backing-identity.c').write_text(mutant)
    binary = out / 'reject-wrong-backing-identity'
    (out / 'reject-wrong-backing-identity-compile.log').write_text(run(cc + ['-DCONFIG_KSU_SUSFS_SUS_PATH=1',
        str(out / 'wrong-backing-identity.c'), '-o', str(binary)]))
    rejected = subprocess.run([str(binary)], capture_output=True, text=True)
    (out / 'reject-wrong-backing-identity.log').write_text(rejected.stdout + rejected.stderr)
    assert rejected.returncode and 'Assertion' in rejected.stderr
    results['mutation_control'] = 'Using d_real_inode exposes the marked overlay view and fails the policy assertion'
    results['unchanged_native_sources'] = list(sources)
    results['production_gate'] = 'SUS_PATH remains rejected by the aggregate and excluded from capabilities'
    paths = [Path(__file__).resolve(), fixture, root / 'fs/susfs_path.c',
             root / 'include/linux/susfs_def.h', root / 'scripts/susfs/test_selinux_adapter.py',
             root / 'scripts/susfs/test_setresuid_task_state.py'] + [root / name for name in sources]
    results['inputs'] = {str(path.relative_to(root)): digest(path) for path in paths}
    (out / 'results.json').write_text(json.dumps(results, indent=2) + '\n')
    print('Overlay identity host checks passed; native/concurrent filesystem integration is still pending.')


if __name__ == '__main__':
    main()
