#!/usr/bin/env python3
"""Compare real exported CRCs and optional installed ARM64 module imports.

Read-only build gate. Matching CRCs do not establish native memory safety,
feature behavior, hardware compatibility or bootability.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def exports(path):
    result = {}
    for line in path.read_text().splitlines():
        fields = line.split()
        if len(fields) < 4:
            raise ValueError('Invalid Module.symvers entry: ' + line)
        crc, name, provider, kind = fields[:4]
        if name in result:
            raise ValueError('Duplicate export: ' + name)
        result[name] = {'crc': int(crc, 16), 'kind': kind,
                        'namespace': fields[4] if len(fields) > 4 else ''}
    if not result or 'module_layout' not in result:
        raise ValueError('Missing native module_layout export')
    return result


def imports(path):
    data = path.read_bytes()
    if len(data) < 64 or data[:6] != b'\x7fELF\x02\x01' or struct.unpack_from('<H', data, 18)[0] != 183:
        raise ValueError('Expected little-endian ARM64 ELF module: ' + str(path))
    start = struct.unpack_from('<Q', data, 40)[0]
    stride, count, names_index = struct.unpack_from('<HHH', data, 58)
    if stride < 64 or not count or names_index >= count or start + count * stride > len(data):
        raise ValueError('Invalid module section table')

    def section(index):
        offset = start + index * stride
        name = struct.unpack_from('<I', data, offset)[0]
        location, size = struct.unpack_from('<QQ', data, offset + 24)
        return name, location, size

    _, location, size = section(names_index)
    names = data[location:location + size]
    for index in range(count):
        name, location, size = section(index)
        if names[name:].split(b'\0', 1)[0] != b'__versions':
            continue
        if size % 64 or location + size > len(data):
            raise ValueError('Invalid __versions records')
        result = {}
        for offset in range(location, location + size, 64):
            crc = struct.unpack_from('<Q', data, offset)[0]
            symbol = data[offset + 8:offset + 64].split(b'\0', 1)[0].decode('ascii')
            if not symbol or symbol in result:
                raise ValueError('Invalid/duplicate module import')
            result[symbol] = crc
        if 'module_layout' not in result:
            raise ValueError('Module has no module_layout version')
        return result
    raise ValueError('Module has no __versions section')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--reference', type=Path, required=True)
    parser.add_argument('--candidate', type=Path, required=True)
    parser.add_argument('--module', type=Path, action='append', default=[])
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    reference, candidate = exports(args.reference), exports(args.candidate)
    common = reference.keys() & candidate.keys()
    missing = sorted(reference.keys() - candidate.keys())
    mismatches = {name: {'reference': reference[name], 'candidate': candidate[name]}
                  for name in sorted(common) if reference[name] != candidate[name]}
    modules = []
    for path in args.module:
        required = imports(path)
        bad = {name: {'installed': hex(crc), 'candidate': hex(candidate[name]['crc']) if name in candidate else None}
               for name, crc in required.items() if name not in candidate or candidate[name]['crc'] != crc}
        modules.append({'path': str(path.resolve()), 'sha256': sha(path),
                        'checked_imports': len(required), 'mismatches': bad})
    passed = not missing and not mismatches and not any(m['mismatches'] for m in modules)
    report = {'status': 'passed' if passed else 'rejected', 'scope': __doc__,
              'reference': str(args.reference.resolve()), 'reference_sha256': sha(args.reference),
              'candidate': str(args.candidate.resolve()), 'candidate_sha256': sha(args.candidate),
              'compared_exports': len(common), 'missing_reference_exports': missing,
              'changed_export_count': len(mismatches), 'changed_exports': mismatches,
              'additional_exports': sorted(candidate.keys() - reference.keys()), 'modules': modules,
              'candidate_module_layout': hex(candidate['module_layout']['crc'])}
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(report, indent=2) + '\n')
    print(report['status'] + ': ' + str(len(common)) + ' exports compared; ' + str(len(mismatches)) +
          ' changed; ' + str(len(missing)) + ' missing; ' +
          str(sum(len(m['mismatches']) for m in modules)) + ' checked module-import mismatches')
    return 0 if passed else 1


if __name__ == '__main__':
    raise SystemExit(main())
