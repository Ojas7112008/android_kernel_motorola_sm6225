#!/usr/bin/env python3
"""Execute real identity handlers/consumers under explicit host IO/lock models.

This does not exercise native UTS, procfs, jump labels or a booted kernel.
"""
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys

sys.dont_write_bytecode = True
from test_setresuid_task_state import section
from test_selinux_adapter import function


def main():
    root = Path(__file__).resolve().parents[2]
    out = root / '.backport-context/audit/all-features/identity-host'
    out.mkdir(parents=True, exist_ok=True)
    inc = out / 'linux'
    inc.mkdir(exist_ok=True)
    header = (root / 'include/linux/susfs.h').read_text()
    defs = (root / 'include/linux/susfs_def.h').read_text()
    payloads = '#pragma once\n'
    for name in ['SUSFS_FAKE_CMDLINE_OR_BOOTCONFIG_SIZE', 'SUSFS_ENABLED_FEATURES_SIZE']:
        payloads += re.search(r'^#define ' + name + r' .*', defs, re.M).group(0) + '\n'
    for name in ['st_susfs_uname', 'st_susfs_spoof_cmdline_or_bootconfig', 'st_susfs_enabled_features']:
        payloads += re.search(r'struct ' + name + r' \{.*?\n\};', header, re.S).group(0) + '\n'
    (inc / 'susfs.h').write_text(payloads)
    (inc / 'susfs_spoof.h').write_bytes((root / 'include/linux/susfs_spoof.h').read_bytes())
    for name in ['errno.h', 'mutex.h', 'string.h', 'uaccess.h', 'utsname.h', 'seq_file.h', 'slab.h', 'types.h']:
        (inc / name).write_text('/* Explicit host model in the fixture prefix. */\n')
    # glibc errno.h uses linux/errno.h; retain the platform error constants.
    (inc / 'errno.h').write_text('#include_next <linux/errno.h>\n')
    (inc / 'types.h').write_text('#include_next <linux/types.h>\n')
    (inc / 'overflow.h').write_text('#define check_add_overflow(a,b,p) __builtin_add_overflow(a,b,p)\n')
    sources = ['fs/susfs_uname.c', 'fs/susfs_cmdline.c', 'fs/proc/cmdline.c',
               'kernel/sys.c', 'kernel/kallsyms.c', 'include/linux/susfs_spoof.h',
               'include/linux/susfs.h', 'include/linux/susfs_def.h', 'fs/susfs.c',
               'scripts/susfs/test_identity_features.py', 'scripts/susfs/test_identity_features.c']
    (out / 'uname_handler.h').write_bytes((root / sources[0]).read_bytes())
    (out / 'cmdline_handler.h').write_bytes((root / sources[1]).read_bytes())
    native_uname = section((root / 'kernel/sys.c').read_text(), 'SYSCALL_DEFINE1(newuname,')
    (out / 'native_uname.h').write_text(native_uname)
    (out / 'native_cmdline.h').write_text(function((root / 'fs/proc/cmdline.c').read_text(), 'cmdline_proc_show'))
    (out / 'native_symbols.h').write_text(function((root / 'kernel/kallsyms.c').read_text(), 's_show'))
    (out / 'features_query.h').write_text(function((root / 'fs/susfs.c').read_text(), 'susfs_get_enabled_features'))
    cc = ['clang', '-std=gnu11', '-O1', '-g', '-pthread', '-D_GNU_SOURCE', '-Wall', '-Wextra', '-Werror',
          '-Wno-unused-function', '-Wno-unused-parameter', '-Wno-unused-variable',
          '-fsanitize=address,undefined', '-fno-sanitize-recover=undefined', '-fno-omit-frame-pointer', '-I', str(out)]
    features = ['SPOOF_UNAME', 'SPOOF_CMDLINE_OR_BOOTCONFIG', 'HIDE_KSU_SUSFS_SYMBOLS',
                'SUS_MAP', 'SUS_KSTAT', 'OPEN_REDIRECT', 'SUS_PATH', 'SUS_MOUNT']
    (out / 'capabilities_expected.h').write_text(''.join(
        '#ifdef CONFIG_KSU_SUSFS_' + feature + '\nassert(strstr(info->enabled_features,"CONFIG_KSU_SUSFS_' + feature + '\\n"));\n'
        '#else\nassert(!strstr(info->enabled_features,"CONFIG_KSU_SUSFS_' + feature + '\\n"));\n#endif\n' for feature in features))
    modes = [[], ['SPOOF_UNAME'], ['SPOOF_CMDLINE_OR_BOOTCONFIG'], ['HIDE_KSU_SUSFS_SYMBOLS'],
             ['SPOOF_UNAME', 'SPOOF_CMDLINE_OR_BOOTCONFIG', 'HIDE_KSU_SUSFS_SYMBOLS'], features]
    report = {'scope': __doc__, 'inputs': {name: hashlib.sha256((root / name).read_bytes()).hexdigest() for name in sources}, 'runs': []}
    for i, mode in enumerate(modes):
        binary = out / ('identity-' + str(i))
        command = cc + ['-DCONFIG_KSU_SUSFS_' + flag + '=1' for flag in mode]
        command += [str(root / 'scripts/susfs/test_identity_features.c'), '-o', str(binary)]
        result = subprocess.run(command, text=True, capture_output=True)
        (out / (binary.name + '-compile.log')).write_text(result.stdout + result.stderr)
        if result.returncode:
            raise RuntimeError(result.stderr)
        result = subprocess.run([str(binary)], text=True, capture_output=True)
        (out / (binary.name + '.log')).write_text(result.stdout + result.stderr)
        assert result.returncode == 0, result.stdout + result.stderr
        report['runs'].append({'features': mode, 'exit': result.returncode, 'result': result.stdout.strip()})
        print(binary.name + ': ' + result.stdout.strip())
    (out / 'results.json').write_text(json.dumps(report, indent=2) + '\n')


if __name__ == '__main__':
    main()
