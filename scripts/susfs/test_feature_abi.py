#!/usr/bin/env python3
"""Compare every command payload field with pinned JNI for ARM64 and ARM32.

Freestanding layout checks do not exercise the supercall transport or runtime.
The kstat handler's ARM32 conversion is covered by test_susfs_kstat.py.
"""
import hashlib
import json
from pathlib import Path
import re
import subprocess


def main():
    root = Path(__file__).resolve().parents[2]
    out = root / '.backport-context/audit/all-features/feature-abi'
    out.mkdir(parents=True, exist_ok=True)
    header = root / 'include/linux/susfs.h'
    defs = root / 'include/linux/susfs_def.h'
    uts = root / 'include/uapi/linux/utsname.h'
    inputs = [header, defs, uts, Path(__file__).resolve()]
    prefix = '#define bool _Bool\n'
    show = root / '.backport-context/repos/susfs/ksu_susfs/jni/features/show.c'
    prefix += '\n'.join(re.findall(r'^#define SUSFS_MAX_(?:VERSION|VARIANT)_BUFSIZE\s+[^\n]+', show.read_text(), re.M)) + '\n'
    for path, names in [(defs, ['SUSFS_MAX_LEN_PATHNAME', 'SUSFS_FAKE_CMDLINE_OR_BOOTCONFIG_SIZE', 'SUSFS_ENABLED_FEATURES_SIZE']),
                        (uts, ['__NEW_UTS_LEN'])]:
        prefix += '\n'.join(re.search(r'^#define\s+' + name + r'\s+[^\n]+', path.read_text(), re.M).group(0) for name in names) + '\n'
    assertions, payloads = '', []
    for path in sorted((root / '.backport-context/repos/susfs/ksu_susfs/jni/features').glob('*.c')):
        for match in re.finditer(r'struct (st_susfs_\w+) \{.*?\n\};', path.read_text(), re.S):
            name = match.group(1)
            kernel = re.search(r'struct ' + name + r' \{.*?\n\};', header.read_text(), re.S).group(0)
            user = match.group(0).replace('struct ' + name, 'struct jni_' + name, 1)
            prefix += kernel + '\n' + user + '\n'
            assertions += '_Static_assert(sizeof(struct ' + name + ')==sizeof(struct jni_' + name + '),"sizeof ' + name + '");\n'
            fields = re.findall(r'^\s*(?:bool|char|unsigned long|unsigned int|long long|long|int)\s+(\w+)(?:\[[^]]+\])?;', kernel, re.M)
            assert fields and 'err' in fields, name
            for field in fields:
                assertions += '_Static_assert(__builtin_offsetof(struct ' + name + ',' + field + ')==__builtin_offsetof(struct jni_' + name + ',' + field + '),"' + name + '.' + field + '");\n'
            inputs.append(path)
            payloads.append({'name': name, 'fields': len(fields)})
    assert len(payloads) == 12
    source = out / 'command-layouts.c'
    source.write_text(prefix + assertions)
    report = {'scope': __doc__, 'payloads': payloads, 'runs': []}
    for target in ['aarch64-linux-android', 'armv7a-linux-androideabi']:
        result = subprocess.run(['clang', '--target=' + target, '-ffreestanding', '-nostdinc', '-std=gnu11',
                                 '-Werror', '-fsyntax-only', str(source)], text=True, capture_output=True)
        (out / (target + '.log')).write_text(result.stdout + result.stderr)
        assert result.returncode == 0, result.stderr
        report['runs'].append({'target': target, 'exit': 0})
        print(target + ': all 12 command payload sizes and field offsets match pinned JNI')
    report['inputs'] = {str(p.relative_to(root)): hashlib.sha256(p.read_bytes()).hexdigest() for p in dict.fromkeys(inputs)}
    (out / 'results.json').write_text(json.dumps(report, indent=2) + '\n')


if __name__ == '__main__':
    main()
