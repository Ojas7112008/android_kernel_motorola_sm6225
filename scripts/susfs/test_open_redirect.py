#!/usr/bin/env python3
"""Real redirect rules, filename wrapper and readlink bodies with host models.

Path walk, filesystem side effects, permission checks and RCU concurrency are modeled.
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
    out = root / '.backport-context/audit/all-features/redirect-host'
    out.mkdir(parents=True, exist_ok=True)
    inc = out / 'linux'
    inc.mkdir(exist_ok=True)
    for name in ['fs.h', 'list.h', 'mutex.h', 'namei.h', 'rcupdate.h', 'rculist.h', 'slab.h', 'string.h', 'uaccess.h', 'types.h']:
        (inc / name).write_text('/* Explicit host model. */\n')
    (inc / 'errno.h').write_text('#include_next <linux/errno.h>\n')
    (inc / 'overflow.h').write_text('#define check_add_overflow(a,b,p) __builtin_add_overflow(a,b,p)\n')
    header = (root / 'include/linux/susfs.h').read_text()
    defs = (root / 'include/linux/susfs_def.h').read_text()
    constants = '\n'.join(re.search(r'^#define ' + name + r' .*', defs, re.M).group(0)
                          for name in ['SUSFS_MAX_LEN_PATHNAME', 'AS_FLAGS_OPEN_REDIRECT', 'FUSE_SUPER_MAGIC', 'TIF_PROC_UMOUNTED'])
    constants += '\n' + section(defs, 'static inline bool susfs_is_current_proc_umounted_app(')
    constants += section(defs, 'static inline bool susfs_is_current_proc_umounted(')
    (inc / 'susfs_def.h').write_text('#pragma once\n' + constants)
    (inc / 'susfs.h').write_text('#pragma once\n#include "susfs_def.h"\n' +
        re.search(r'enum UID_SCHEME \{.*?\n\};', header, re.S).group(0) + '\n' +
        re.search(r'struct st_susfs_open_redirect \{.*?\n\};', header, re.S).group(0) + '\n')
    (inc / 'susfs_redirect.h').write_bytes((root / 'include/linux/susfs_redirect.h').read_bytes())
    (out / 'redirect_handler.h').write_bytes((root / 'fs/susfs_redirect.c').read_bytes())
    namei = (root / 'fs/namei.c').read_text()
    (out / 'native_nameidata.h').write_text(function(namei, 'set_nameidata') + function(namei, 'restore_nameidata'))
    (out / 'native_resolver.h').write_text(function(namei, 'susfs_redirect_resolved_path'))
    (out / 'native_wrapper.h').write_text(function(namei, 'do_filp_open'))
    (out / 'native_readlink.h').write_text(function(namei, 'vfs_readlink'))
    (out / 'native_proc_readlink.h').write_text(function((root / 'fs/proc/base.c').read_text(), 'do_proc_readlink'))
    # Consumer placement is checked in addition to execution of wrapper/rule bodies.
    last = function(namei, 'do_last')
    assert last.index('complete_walk(nd)') < last.index('susfs_redirect_resolved_path') < last.index('if (will_truncate)') < last.index('vfs_open(')
    assert last.index('== (O_EXCL | O_CREAT)') < last.index('susfs_redirect_resolved_path')
    lookup = function(namei, 'lookup_open')
    assert lookup.index('susfs_redirect_may_apply') < lookup.index('if (dir_inode->i_op->atomic_open)')
    for name, effect in [('do_tmpfile', 'mnt_want_write'), ('do_o_path', 'vfs_open')]:
        body = function(namei, name)
        assert body.index('susfs_redirect_resolved_path') < body.index(effect)
    cc = ['clang', '-std=gnu11', '-O1', '-g', '-D_GNU_SOURCE', '-Wall', '-Wextra', '-Werror',
          '-Wno-unused-function', '-Wno-unused-parameter', '-Wno-unused-variable', '-Wno-sign-compare',
          '-fsanitize=address,undefined', '-fno-sanitize-recover=undefined', '-fno-omit-frame-pointer', '-I', str(out)]
    sources = ['fs/susfs_redirect.c', 'include/linux/susfs_redirect.h', 'include/linux/susfs.h', 'include/linux/susfs_def.h',
               'fs/namei.c', 'fs/proc/base.c', 'fs/proc/task_mmu.c', 'scripts/susfs/test_open_redirect.py', 'scripts/susfs/test_open_redirect.c']
    report = {'scope': __doc__, 'inputs': {p: hashlib.sha256((root / p).read_bytes()).hexdigest() for p in sources}, 'runs': []}
    for enabled in [False, True]:
        name = 'redirect-' + str(int(enabled))
        command = cc + (['-DCONFIG_KSU_SUSFS_OPEN_REDIRECT=1'] if enabled else [])
        result = subprocess.run(command + [str(root / 'scripts/susfs/test_open_redirect.c'), '-o', str(out / name)], text=True, capture_output=True)
        (out / (name + '-compile.log')).write_text(result.stdout + result.stderr)
        assert result.returncode == 0, result.stderr
        result = subprocess.run([str(out / name)], text=True, capture_output=True)
        (out / (name + '.log')).write_text(result.stdout + result.stderr)
        assert result.returncode == 0, result.stdout + result.stderr
        report['runs'].append({'name': name, 'exit': 0, 'result': result.stdout.strip()})
        print(name + ': ' + result.stdout.strip())
    (out / 'results.json').write_text(json.dumps(report, indent=2) + '\n')


if __name__ == '__main__':
    main()
