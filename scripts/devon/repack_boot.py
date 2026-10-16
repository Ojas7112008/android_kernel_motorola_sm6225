#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0
"""Replace the kernel in the exact supplied Axion boot v3 / unsigned AVB image.

The layout follows the earlier devon-resukisu workflow. This deliberately rejects
other base images and signed AVB; it never changes verification flags or a ramdisk.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import struct
import tempfile

BASE_SHA256 = '1632d107116564f6130b8633c5c21b09261009a03712e086dd22cb9e71edc4de'
PARTITION_SIZE = 96 * 1024 * 1024
PAGE = 4096


def require(ok, message):
    if not ok:
        raise ValueError(message)


def sha(data):
    return hashlib.sha256(data).hexdigest()


def align(size):
    return (size + PAGE - 1) // PAGE * PAGE


def parse_boot(data):
    """Validate layout and the boot hash descriptor, independent of base identity."""
    require(len(data) == PARTITION_SIZE, 'Expected the 96 MiB boot partition image')
    require(data[:8] == b'ANDROID!', 'Invalid Android boot magic')
    kernel_size, ramdisk_size, _, header_size = struct.unpack_from('<4I', data, 8)
    require(struct.unpack_from('<I', data, 40)[0] == 3 and header_size == 1580,
            'Only the supplied Android boot header v3 is supported')
    require(kernel_size > 1024 * 1024 and ramdisk_size > 0, 'Invalid component sizes')
    rd_start = PAGE + align(kernel_size)
    rd_end = rd_start + ramdisk_size
    image_end = rd_start + align(ramdisk_size)
    require(rd_end <= image_end <= len(data) - 64, 'Component exceeds partition bounds')
    require(data[rd_start:rd_start + 3] == b'\x1f\x8b\x08', 'Expected original gzip ramdisk')
    footer = len(data) - 64
    require(data[footer:footer + 4] == b'AVBf', 'Missing AVB footer')
    major, minor, original_size, vb_offset, vb_size = struct.unpack_from('>IIQQQ', data, footer + 4)
    require((major, minor) == (1, 0), 'Unsupported AVB footer version')
    require(original_size == image_end == vb_offset, 'Unexpected boot/ramdisk/AVB layout')
    require(vb_size >= 256 and vb_offset + vb_size <= footer, 'AVB metadata exceeds partition')
    require(data[vb_offset:vb_offset + 4] == b'AVB0', 'Invalid vbmeta magic')
    auth, aux = struct.unpack_from('>QQ', data, vb_offset + 12)
    algorithm = struct.unpack_from('>I', data, vb_offset + 28)[0]
    require(auth == 0 and algorithm == 0, 'Signed AVB is unsupported; signing is never removed')
    require(vb_size == 256 + aux and aux % 64 == 0, 'Invalid AVB auxiliary size')
    require(not any(data[vb_offset + vb_size:footer]), 'Nonzero data after vbmeta')
    desc_offset, desc_size = struct.unpack_from('>QQ', data, vb_offset + 96)
    require(desc_offset + desc_size <= aux, 'AVB descriptors exceed auxiliary block')
    position = vb_offset + 256 + desc_offset
    descriptors_end = position + desc_size
    boot_hashes = []
    while position < descriptors_end:
        require(position + 16 <= descriptors_end, 'Truncated AVB descriptor')
        tag, extra = struct.unpack_from('>QQ', data, position)
        end = position + 16 + extra
        require(extra % 8 == 0 and end <= descriptors_end, 'Invalid AVB descriptor bounds')
        if tag == 2:
            require(extra >= 116, 'Short AVB hash descriptor')
            image_size = struct.unpack_from('>Q', data, position + 16)[0]
            algorithm_name = data[position + 24:position + 56].split(b'\0', 1)[0]
            name_len, salt_len, digest_len, _ = struct.unpack_from('>4I', data, position + 56)
            payload = position + 132
            require(payload + name_len + salt_len + digest_len <= end, 'Invalid AVB hash fields')
            name = data[payload:payload + name_len]
            salt = bytes(data[payload + name_len:payload + name_len + salt_len])
            digest_at = payload + name_len + salt_len
            require(name == b'boot' and image_size == image_end, 'Unexpected AVB partition or image size')
            require(algorithm_name == b'sha256' and digest_len == 32, 'Unsupported AVB digest')
            require(hashlib.sha256(salt + data[:image_size]).digest() == data[digest_at:digest_at + 32],
                    'AVB boot digest does not match the image')
            boot_hashes.append({'descriptor': position - vb_offset,
                                'digest': digest_at - vb_offset, 'salt': salt})
        position = end
    require(position == descriptors_end and len(boot_hashes) == 1,
            'Expected exactly one boot hash descriptor')
    return {'kernel_size': kernel_size, 'ramdisk_size': ramdisk_size,
            'ramdisk_start': rd_start, 'ramdisk_end': rd_end, 'image_end': image_end,
            'vbmeta_offset': vb_offset, 'vbmeta_size': vb_size, 'hash': boot_hashes[0]}


def load_base(path):
    data = Path(path).read_bytes()
    require(sha(data) == BASE_SHA256, 'Base SHA-256 differs from the supplied Axion boot.img')
    return data, parse_boot(data)


def repack_bytes(base, kernel):
    old = parse_boot(base)
    require(len(kernel) > 1024 * 1024 and kernel[56:60] == b'ARM\x64',
            'Kernel must be a raw ARM64 Linux Image')
    new_end = PAGE + align(len(kernel)) + align(old['ramdisk_size'])
    require(new_end + align(old['vbmeta_size']) + PAGE <= len(base),
            'Kernel does not fit with the AVB metadata/footer in the boot partition')
    output = bytearray(len(base))
    output[:PAGE] = base[:PAGE]
    struct.pack_into('<I', output, 8, len(kernel))
    output[PAGE:PAGE + len(kernel)] = kernel
    rd_start = PAGE + align(len(kernel))
    output[rd_start:new_end] = base[old['ramdisk_start']:old['image_end']]
    output[new_end:new_end + old['vbmeta_size']] = base[old['vbmeta_offset']:old['vbmeta_offset'] + old['vbmeta_size']]
    output[-64:] = base[-64:]
    struct.pack_into('>QQ', output, len(output) - 52, new_end, new_end)
    descriptor = new_end + old['hash']['descriptor']
    digest_at = new_end + old['hash']['digest']
    struct.pack_into('>Q', output, descriptor + 16, new_end)
    output[digest_at:digest_at + 32] = hashlib.sha256(old['hash']['salt'] + output[:new_end]).digest()
    checked = parse_boot(output)
    require(output[PAGE:PAGE + len(kernel)] == kernel, 'Kernel payload changed')
    require(output[:8] == base[:8] and output[12:PAGE] == base[12:PAGE], 'Boot header changed unexpectedly')
    require(output[checked['ramdisk_start']:checked['image_end']] == base[old['ramdisk_start']:old['image_end']],
            'Ramdisk or ramdisk padding changed')
    # All vbmeta bytes except the boot descriptor size/digest must be identical.
    normalized = bytearray(output[new_end:new_end + old['vbmeta_size']])
    original_vbmeta = base[old['vbmeta_offset']:old['vbmeta_offset'] + old['vbmeta_size']]
    for offset, size in [(old['hash']['descriptor'] + 16, 8), (old['hash']['digest'], 32)]:
        normalized[offset:offset + size] = original_vbmeta[offset:offset + size]
    require(normalized == original_vbmeta, 'AVB metadata or verification flags changed unexpectedly')
    require(output[-64:-52] == base[-64:-52] and output[-36:] == base[-36:], 'AVB footer changed unexpectedly')
    return output


def validate_artifacts(directory, commit):
    directory = Path(directory).resolve(strict=True)
    record = json.loads((directory / 'build-result.json').read_text())
    require(record['bakasu'] == commit, 'Saved kernel was not built with the selected BakaSU commit')
    require(record['linked_bakasu_symbols'] > 0 and record['kernel_modules'] > 0,
            'Missing successful BakaSU build evidence')
    for name in ('Image', 'kernel.config', 'bakasu-symbols.txt', 'dtbs.tar.gz', 'modules.tar.gz'):
        path = directory / name
        require(path.resolve().is_relative_to(directory), 'Artifact leaves its directory')
        expected = record['artifacts'][name]
        with path.open('rb') as stream:
            digest = hashlib.file_digest(stream, 'sha256').hexdigest()
        require(path.stat().st_size == expected['bytes'] and digest == expected['sha256'],
                f'Artifact checksum mismatch: {name}')
    config = (directory / 'kernel.config').read_text().splitlines()
    for option in ('KSU', 'KSU_MANUAL_HOOK', 'KSU_MANUAL_HOOK_AUTO_SETUID_HOOK',
                   'KSU_MANUAL_HOOK_AUTO_INITRC_HOOK', 'KSU_MANUAL_HOOK_AUTO_INPUT_HOOK',
                   'LTO_CLANG', 'CFI_CLANG'):
        require(f'CONFIG_{option}=y' in config, f'Missing configuration: {option}')
    require('CONFIG_KSU_SUSFS=y' not in config and 'CONFIG_KSU_TRACEPOINT_HOOK=y' not in config,
            'Unexpected SUSFS/tracepoint configuration')
    require('CONFIG_CFI_PERMISSIVE=y' not in config, 'CFI must remain enforcing')
    return (directory / 'Image').read_bytes(), record


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--base', required=True, type=Path)
    parser.add_argument('--kernel-artifacts', type=Path)
    parser.add_argument('--bakasu-commit')
    parser.add_argument('--output', type=Path)
    parser.add_argument('--report', type=Path)
    args = parser.parse_args()
    base, layout = load_base(args.base)
    if not args.kernel_artifacts:
        require(not any((args.output, args.report, args.bakasu_commit)), 'Missing --kernel-artifacts')
        print(json.dumps({'base_sha256': sha(base), 'partition_bytes': len(base),
                          'header_version': 3, 'avb_algorithm': 'NONE (preserved)',
                          'kernel_bytes': layout['kernel_size'], 'ramdisk_bytes': layout['ramdisk_size']}, indent=2))
        return
    require(args.output and args.report and args.bakasu_commit, 'Missing output/report/BakaSU arguments')
    require(args.output.resolve() != args.base.resolve(), 'Output cannot replace the input boot image')
    require(not args.output.exists() and not args.report.exists(), 'Output/report already exists')
    kernel, build = validate_artifacts(args.kernel_artifacts, args.bakasu_commit)
    output = repack_bytes(base, kernel)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    fd, temporary = tempfile.mkstemp(prefix='.boot-', suffix='.partial', dir=args.output.parent)
    try:
        with os.fdopen(fd, 'wb') as stream:
            stream.write(output)
            stream.flush()
            os.fsync(stream.fileno())
        # Atomic publication without overwriting an existing path.
        os.link(temporary, args.output)
    finally:
        os.unlink(temporary)
    require(sha(args.base.read_bytes()) == BASE_SHA256, 'Input boot image changed during repacking')
    report = {'base': str(args.base.resolve()), 'base_sha256': BASE_SHA256,
              'output': str(args.output.resolve()), 'output_sha256': sha(output),
              'partition_bytes': len(output), 'kernel_sha256': sha(kernel),
              'kernel_bytes': len(kernel), 'ramdisk_sha256': sha(base[layout['ramdisk_start']:layout['ramdisk_end']]),
              'ramdisk_preserved': True, 'header_and_avb_flags_preserved': True,
              'avb_boot_hash_verified': True, 'avb_algorithm': 'NONE (unchanged)',
              'bakasu_commit': build['bakasu'], 'kernel_checkout': build['kernel_checkout'],
              'kernel_artifacts': str(args.kernel_artifacts.resolve()),
              'validation_scope': 'Compile evidence and image structure/hash validation; no boot or runtime test'}
    args.report.write_text(json.dumps(report, indent=2) + '\n')
    args.output.with_suffix('.img.sha256').write_text(sha(output) + '  ' + args.output.name + '\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    try:
        main()
    except (ValueError, OSError, KeyError) as error:
        raise SystemExit(f'ERROR: {error}')
