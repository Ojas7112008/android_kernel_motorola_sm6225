#!/usr/bin/env python3
"""Native map handlers and traversal bodies under host VFS/MM/page models.

No real page tables, mmap locking, ptrace permissions, device or runtime proof.
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
    out = root / '.backport-context/audit/all-features/map-host'
    out.mkdir(parents=True, exist_ok=True)
    inc = out / 'linux'
    inc.mkdir(exist_ok=True)
    for name in ['fs.h', 'mm.h', 'namei.h', 'string.h', 'uaccess.h']:
        (inc / name).write_text('/* Explicit fixture model. */\n')
    (inc / 'errno.h').write_text('#include_next <linux/errno.h>\n')
    (inc / 'overflow.h').write_text('#define check_add_overflow(a,b,p) __builtin_add_overflow(a,b,p)\n')
    defs = (root / 'include/linux/susfs_def.h').read_text()
    macro_names = ['AS_FLAGS_SUS_MAP', 'TIF_PROC_UMOUNTED', 'SUSFS_MAX_LEN_PATHNAME']
    (inc / 'susfs_def.h').write_text('#pragma once\n' + '\n'.join(re.search(r'^#define ' + name + r' .*', defs, re.M).group(0) for name in macro_names) + '\n' + section(defs, 'static inline bool susfs_is_current_proc_umounted_app('))
    header = (root / 'include/linux/susfs.h').read_text()
    (inc / 'susfs.h').write_text('#pragma once\n#include "susfs_def.h"\n' + re.search(r'struct st_susfs_sus_map \{.*?\n\};', header, re.S).group(0) + '\n')
    (inc / 'susfs_map.h').write_bytes((root / 'include/linux/susfs_map.h').read_bytes())
    (out / 'map_handler.h').write_bytes((root / 'fs/susfs_map.c').read_bytes())
    mmu = (root / 'fs/proc/task_mmu.c').read_text()
    (out / 'native_pagemap.h').write_text(section(mmu, 'static inline pagemap_entry_t make_pme(') +
        ''.join(function(mmu, name) for name in ['add_to_pagemap', 'susfs_pagemap_hidden_range', 'pagemap_pte_hole']))
    (out / 'native_rollup.h').write_text(function(mmu, 'show_smaps_rollup'))
    (out / 'native_remote.h').write_text(function((root / 'mm/memory.c').read_text(), '__access_remote_vm'))
    base = (root / 'fs/proc/base.c').read_text()
    (out / 'native_map_files.h').write_text(function(base, 'proc_map_files_readdir'))
    inputs = ['fs/susfs_map.c', 'include/linux/susfs_map.h', 'include/linux/susfs.h', 'include/linux/susfs_def.h',
              'fs/proc/task_mmu.c', 'fs/proc/base.c', 'mm/memory.c', 'scripts/susfs/test_susfs_map.py', 'scripts/susfs/test_susfs_map.c']
    report = {'scope': __doc__, 'inputs': {name: hashlib.sha256((root / name).read_bytes()).hexdigest() for name in inputs}, 'runs': []}
    cc = ['clang', '-std=gnu11', '-O1', '-g', '-D_GNU_SOURCE', '-Wall', '-Wextra', '-Werror',
          '-Wno-unused-function', '-Wno-unused-parameter', '-Wno-unused-variable', '-Wno-sign-compare',
          '-fsanitize=address,undefined', '-fno-sanitize-recover=undefined', '-fno-omit-frame-pointer', '-I', str(out)]
    for enabled in [False, True]:
        for io in [False, True]:
            name = f'map-{int(enabled)}-io-{int(io)}'
            flags = (['-DCONFIG_KSU_SUSFS_SUS_MAP=1'] if enabled else []) + (['-DCONFIG_HAVE_IOREMAP_PROT=1'] if io else [])
            result = subprocess.run(cc + flags + [str(root / 'scripts/susfs/test_susfs_map.c'), '-o', str(out / name)], text=True, capture_output=True)
            (out / (name + '-compile.log')).write_text(result.stdout + result.stderr)
            assert result.returncode == 0, result.stderr
            result = subprocess.run([str(out / name)], text=True, capture_output=True)
            (out / (name + '.log')).write_text(result.stdout + result.stderr)
            assert result.returncode == 0, result.stdout + result.stderr
            report['runs'].append({'name': name, 'exit': 0, 'result': result.stdout.strip()})
            print(name + ': ' + result.stdout.strip())
    report['negative_controls'] = []
    mutations = [
        ('remote-boundary', 'native_remote.h',
         'if (vma && addr >= vma->vm_start && susfs_is_hidden_vma(vma))', 'if (false)'),
        ('pagemap-zeroes', 'native_pagemap.h', 'empty = make_pme(0, 0)', 'empty = make_pme(1, PM_SOFT_DIRTY)'),
        ('map-files-initialization', 'native_map_files.h', 'nr_files = i;', '(void)i;'),
        ('rollup-filter', 'native_rollup.h', 'if (susfs_is_hidden_vma(vma))', 'if (false)'),
    ]
    for name, filename, old, new in mutations:
        path = out / filename
        original = path.read_text()
        assert original.count(old) == 1
        try:
            path.write_text(original.replace(old, new, 1))
            binary = out / ('negative-' + name)
            result = subprocess.run(cc + ['-DCONFIG_KSU_SUSFS_SUS_MAP=1', '-DCONFIG_HAVE_IOREMAP_PROT=1',
                str(root / 'scripts/susfs/test_susfs_map.c'), '-o', str(binary)], text=True, capture_output=True)
            assert result.returncode == 0, result.stderr
            result = subprocess.run([str(binary)], text=True, capture_output=True)
            (out / (binary.name + '.log')).write_text(result.stdout + result.stderr)
            assert result.returncode != 0 and 'Assertion' in result.stderr, result.stderr
            report['negative_controls'].append({'name': name, 'exit': result.returncode, 'expected_failure': True})
        finally:
            path.write_text(original)
    (out / 'results.json').write_text(json.dumps(report, indent=2) + '\n')


if __name__ == '__main__':
    main()
