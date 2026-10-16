#!/usr/bin/env python3
"""Actual merged/direct/translated overlay iteration under host VFS models.

No mounted overlay, backing filesystem IO, kernel RCU or native locking is run.
Kernel lock ordering, filesystem IO and runtime performance remain unvalidated.
"""
import json
from pathlib import Path
import re
from kernel_baseline import kernel_source
import subprocess
import sys
sys.dont_write_bytecode = True
from test_path_consumers import prepare, without_hooks
from test_selinux_adapter import function, run, digest
from test_setresuid_task_state import section
from source_guards import without_feature_guards


def overlay_without_hooks(text):
    return without_feature_guards(text, ['CONFIG_KSU_SUSFS_SUS_PATH'])


def main():
    root = Path(__file__).resolve().parents[2]
    out = root / '.backport-context/audit/overlay-merged'
    prepare(root, out)
    namei = (root / 'fs/namei.c').read_text()
    header = (root / 'include/linux/namei.h').read_text()
    source = (root / 'fs/overlayfs/readdir.c').read_text()
    assert without_hooks(header).encode() == kernel_source(root, 'include/linux/namei.h').encode()
    assert overlay_without_hooks(source).encode() == kernel_source(root, 'fs/overlayfs/readdir.c').encode()
    # The private high bit cannot collide with this tree's native lookup flags.
    assert '#define SUSFS_LOOKUP_METADATA (1U << 31)' in namei
    for value in re.findall(r'^#define LOOKUP_\w+\s+(0x[0-9a-fA-F]+)', header, flags=re.M):
        assert not int(value, 16) & (1 << 31)
    common = ''.join(function(namei, n) for n in ['lookup_one_len_common', 'lookup_one_len2', 'lookup_one_len'])
    common += '#ifdef CONFIG_KSU_SUSFS_SUS_PATH\n' + function(namei, 'susfs_lookup_one_len_metadata') + '#endif\n'
    (out / 'native_metadata_lookup.h').write_text(common)
    structures = ''.join(re.search(r'struct ' + n + r'\s*\{.*?\n\};', source, re.S).group(0) + '\n'
                         for n in ['ovl_cache_entry', 'ovl_dir_cache', 'ovl_readdir_data', 'ovl_dir_file', 'ovl_readdir_translate'])
    structures += '#ifdef CONFIG_KSU_SUSFS_SUS_PATH\n' + re.search(r'struct ovl_susfs_collect\s*\{.*?\n\};', source, re.S).group(0) + '\n#endif\n'
    (out / 'native_overlay_dir_structs.h').write_text(structures)
    code = section(source, 'static u64 ovl_remap_lower_ino(')
    code += function(source, 'ovl_seek_cursor')
    code += section(source, 'static int ovl_cache_update_ino(')
    code += function(source, 'ovl_dir_read_impure')
    code += function(source, 'ovl_dir_reset') + function(source, 'ovl_fill_real') + function(source, 'ovl_iterate_real')
    code += '#ifdef CONFIG_KSU_SUSFS_SUS_PATH\n' + ''.join(function(source, n) for n in
        ['ovl_susfs_name_hidden', 'ovl_susfs_entry_hidden', 'ovl_susfs_collect_one', 'ovl_susfs_iterate_real']) + '#endif\n'
    code += function(source, 'ovl_iterate') + section(source, 'static loff_t ovl_dir_llseek(')
    (out / 'native_overlay_readdir.h').write_text(code)
    # Reuse the explicit VFS model, recording flags at the filesystem boundary.
    model = (root / 'scripts/susfs/test_path_consumers.c').read_text()
    model = model.replace('(void)d; (void)flags; int r=reval;', '(void)d; last_reval_flags=flags; int r=reval;')
    model = model.replace('(void)i; (void)flags; lookup_calls++;', '(void)i; last_lookup_flags=flags; lookup_calls++;')
    model = model.replace('assert(m==&mount); permissions++;', 'assert(m==&mount || !m); permissions++;')
    model = model.replace('permissions++;', 'assert(!backing_actor_depth); permissions++;')
    model = model.replace('unsigned long s_magic, s_flags;', 'unsigned long s_magic, s_flags; unsigned s_dev;')
    model = model.replace('static void dput(struct dentry *d) { assert', 'static void dput(struct dentry *d) { if (!d) return; assert')
    assert 'last_lookup_flags=flags' in model and 'last_reval_flags=flags' in model
    (out / 'consumer_fixture.h').write_text(model)
    # Extract native list operations, not alternate production policy code.
    lists = (root / 'include/linux/list.h').read_text()
    list_code = ''.join(function(lists, n) for n in ['INIT_LIST_HEAD', '__list_add', 'list_add_tail', '__list_del', '__list_del_entry', 'list_del'])
    from test_path_consumers import native_macro
    list_code += ''.join(native_macro(lists, n) for n in ['list_entry', 'list_entry_is_head', 'list_first_entry', 'list_next_entry', 'list_for_each', 'list_for_each_entry_safe'])
    (out / 'native_overlay_lists.h').write_text(list_code)
    overlay_header = (root / 'fs/overlayfs/overlayfs.h').read_text()
    constants = re.search(r'enum ovl_path_type\s*\{.*?\};', overlay_header, re.S).group(0) + '\n'
    constants += ''.join(native_macro(overlay_header, n) for n in ['OVL_TYPE_UPPER', 'OVL_TYPE_MERGE', 'OVL_TYPE_ORIGIN'])
    constants += native_macro((root / 'include/uapi/linux/stat.h').read_text(), 'STATX_INO')
    (out / 'native_overlay_dir_constants.h').write_text(constants)
    (out / 'native_dir_emit.h').write_text(function((root / 'include/linux/fs.h').read_text(), 'dir_emit'))
    fixture = root / 'scripts/susfs/test_overlay_readdir.c'
    cc = ['clang', '-std=gnu11', '-O1', '-g', '-Wall', '-Wextra', '-Werror', '-Wno-unused-function',
          '-Wno-unused-parameter', '-Wno-sign-compare', '-fsanitize=address,undefined',
          '-fno-sanitize-recover=undefined', '-fno-omit-frame-pointer', '-pthread', '-I', str(out),
          '-I', str(fixture.parent)]
    base = ['-DCONFIG_ARM64=1', '-DCONFIG_64BIT=1', '-DCONFIG_THREAD_INFO_IN_TASK=1']
    enabled = base + ['-DCONFIG_KSU_SUSFS_SUS_PATH=1', '-DCONFIG_KSU_SUSFS=1', '-DCONFIG_KSU=1', '-DCONFIG_KSU_DEVON_SELINUX_POLICY=1']
    result = {'scope': __doc__, 'compiler': run(['clang', '--version']).splitlines()[0], 'runs': []}
    for path in (False, True):
        for compat in (False, True):
            name = f'overlay-merged-path-{int(path)}-compat-{int(compat)}'
            flags = (enabled if path else base) + (['-DCONFIG_COMPAT=1'] if compat else [])
            (out / (name + '-compile.log')).write_text(run(cc + flags + [str(fixture), '-o', str(out / name)]))
            stdout = run([str(out / name)])
            (out / (name + '.log')).write_text(stdout)
            result['runs'].append({'name': name, 'exit': 0, 'result': stdout.strip()})
            print(name + ': ' + stdout.strip())
    mutations = [
        ('reject-policy-in-shared-cache', 'native_overlay_readdir.h',
         'this = susfs_lookup_one_len_metadata(p->name, dir, p->len);', 'this = lookup_one_len(p->name, dir, p->len);'),
        ('reject-cold-merged-entry-leak', 'native_overlay_readdir.h',
         'err = ovl_susfs_entry_hidden(dentry, p);', 'err = 0;'),
        ('reject-metadata-flag-leak', 'namei_under_test.h', 'flags &= ~SUSFS_LOOKUP_METADATA;', '/* private flag leaked */'),
        ('reject-metadata-permission-bypass', 'native_metadata_lookup.h',
         'err = lookup_one_len_common(name, NULL, base, len, &this);',
         'this = (struct qstr){ .name = name, .len = len }; err = 0;'),
        ('reject-pending-record-loss', 'native_overlay_readdir.h',
         'return 0;\n\t\t/* Consume only after filtering or successful publication. */',
         '{ od->susfs_pending = false; return 0; }\n\t\t/* Consume only after filtering or successful publication. */'),
        ('reject-pending-seek-position', 'native_overlay_readdir.h',
         'res = file->f_pos;', 'res = od->realfile->f_pos;'),
        ('reject-direct-lookup-error-leak', 'native_overlay_readdir.h',
         'err = ovl_susfs_name_hidden(dir, od->susfs_name, od->susfs_len);', 'err = 0;'),
    ]
    for name, target, old, new in mutations:
        original = (out / target).read_text()
        assert old in original
        try:
            (out / target).write_text(original.replace(old, new))
            (out / (name + '-compile.log')).write_text(run(cc + enabled + [str(fixture), '-o', str(out / name)]))
            rejected = subprocess.run([str(out / name)], capture_output=True, text=True)
            (out / (name + '.log')).write_text(rejected.stdout + rejected.stderr)
            assert rejected.returncode and 'Assertion' in rejected.stderr, name
        finally:
            (out / target).write_text(original)
    # overlayfs may be a GPL module while the checked SUSFS core stays built-in.
    assert 'EXPORT_SYMBOL_GPL(susfs_lookup_one_len_metadata);' in namei
    assert 'EXPORT_SYMBOL_GPL(susfs_is_inode_sus_path);' in (root / 'fs/susfs_path.c').read_text()
    (out / 'linux/namei.h').write_text(header)
    (out / 'linux/fcntl.h').write_text('#define AT_FDCWD (-100)\n')
    probe = '''#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>
#include <string.h>
#define __user
#define likely(x) (x)
#define min(a,b) ((a)<(b)?(a):(b))
typedef uint64_t u64;
typedef uint32_t u32;
typedef struct { unsigned val; } kuid_t;
struct inode; struct dentry; struct path; struct vfsmount;
struct list_head { struct list_head *next, *prev; };
static kuid_t current_uid(void) { return (kuid_t){0}; }
static bool test_thread_flag(int n) { (void)n; return false; }
static void set_thread_flag(int n) { (void)n; }
static void clear_thread_flag(int n) { (void)n; }
#include <linux/namei.h>
#include <linux/susfs.h>
struct dentry *(*lookup_signature)(const char *,struct dentry *,int)=susfs_lookup_one_len_metadata;
bool (*predicate_signature)(struct inode *)=susfs_is_inode_sus_path;
'''
    (out / 'overlay-caller.c').write_text(probe)
    for module in (False, True):
        name = 'overlay-caller-module-' + str(int(module))
        (out / (name + '.log')).write_text(run(cc + enabled + (['-DMODULE=1'] if module else []) +
            ['-fsyntax-only', str(out / 'overlay-caller.c')]))
    bad = subprocess.run(cc + enabled + ['-x', 'c', '-fsyntax-only', '-'],
                         input=probe + 'void *susfs_lookup_one_len_metadata(void *);\n', capture_output=True, text=True)
    (out / 'reject-metadata-prototype.log').write_text(bad.stdout + bad.stderr)
    assert bad.returncode and 'conflicting types' in bad.stderr
    result['mutation_controls'] = [m[0] for m in mutations]
    result['caller_syntax'] = 'Full actual namei/susfs declarations compile for built-in and MODULE consumers; wrong metadata prototype is rejected; both GPL exports are present. No target modpost/link is claimed.'
    result['source_identity'] = 'Removing guarded additions restores native overlay readdir and namei header; consumer runner also restores native namei/readdir'
    result['production_gate'] = 'SUS_PATH has conditional objects and capability output; native build/runtime validation is separate'
    inputs = ['scripts/susfs/test_overlay_readdir.py', 'scripts/susfs/test_overlay_readdir.c',
              'scripts/susfs/test_path_consumers.py', 'scripts/susfs/test_path_consumers.c',
              'scripts/susfs/test_selinux_adapter.py', 'scripts/susfs/test_setresuid_task_state.py',
              'fs/namei.c', 'include/linux/namei.h', 'fs/overlayfs/readdir.c', 'fs/readdir.c',
              'fs/susfs_path.c', 'fs/susfs.c', 'include/linux/susfs.h', 'include/linux/susfs_def.h',
              'include/linux/fs.h', 'include/linux/dcache.h', 'include/linux/list.h', 'include/linux/hashtable.h',
              'fs/susfs_fuse.c', 'include/linux/susfs_fuse.h', 'scripts/susfs/fuse_index_model.h',
              'include/linux/ksu_susfs.h', 'scripts/susfs/source-pins.json',
              'fs/overlayfs/overlayfs.h', 'include/uapi/linux/stat.h', 'include/linux/export.h', 'scripts/susfs/source_guards.py']
    result['inputs'] = {n: digest(root / n) for n in inputs}
    (out / 'results.json').write_text(json.dumps(result, indent=2) + '\n')
    print('Merged/direct overlay host checks passed; native concurrency, IO/performance and runtime remain unvalidated.')


if __name__ == '__main__':
    main()
