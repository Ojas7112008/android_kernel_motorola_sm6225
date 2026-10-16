#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0
"""Publish exactly one selected payload and its SHA256 after build validation."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import struct
import tempfile
from axion_boot_layout import load_base, parse_boot, repack_bytes


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def require(ok, message):
    if not ok:
        raise ValueError(message)


def checked_image(artifacts, variant):
    image = artifacts / 'Image'
    with image.open('rb') as stream:
        header = stream.read(64)
    require(len(header) == 64 and header[56:60] == b'ARM\x64' and
            struct.unpack_from('<Q', header, 16)[0] > 0 and image.stat().st_size > 1024 * 1024,
            'Expected a raw ARM64 kernel Image')
    if variant == 'susfs':
        record = json.loads((artifacts / 'result.json').read_text())
        require(record['status'] == 'compiled-and-linked' and record['profile'] == 'all',
                'Expected a successful all-feature SUSFS build')
        expected = json.loads((artifacts / 'SHA256SUMS.json').read_text())['Image']
        expected_config = json.loads((artifacts / 'SHA256SUMS.json').read_text())['kernel.config']
        require(record['features'] == json.loads((artifacts / 'build-pins.json').read_text())['profiles']['all'],
                'SUSFS feature selection differs from its build pins')
    else:
        record = json.loads((artifacts / 'build-result.json').read_text())
        require(record['linked_bakasu_symbols'] > 0 and record['kernel_modules'] > 0,
                'Expected successful BakaSU build evidence')
        expected = record['artifacts']['Image']['sha256']
        expected_config = record['artifacts']['kernel.config']['sha256']
        require(image.stat().st_size == record['artifacts']['Image']['bytes'], 'Kernel size differs from build evidence')
    require(digest(image) == expected, 'Kernel checksum differs from build evidence')
    require(digest(artifacts / 'kernel.config') == expected_config, 'Configuration checksum differs from build evidence')
    config = set((artifacts / 'kernel.config').read_text().splitlines())
    for key in ['ARM64', 'KSU', 'LTO_CLANG', 'CFI_CLANG', 'MODVERSIONS']:
        require('CONFIG_' + key + '=y' in config, 'Missing build option: ' + key)
    require('CONFIG_CFI_PERMISSIVE=y' not in config, 'CFI must remain enforcing')
    require(('CONFIG_KSU_SUSFS=y' in config) == (variant == 'susfs'), 'Wrong source variant')
    require(('CONFIG_KSU_MANUAL_HOOK=y' in config) == (variant == 'bakasu'), 'Wrong hook configuration')
    return image


def package(artifacts, variant, output_format, destination, base=None):
    require(output_format in ('kernel', 'boot.img'), 'Unknown output format')
    require(not destination.exists(), 'Output directory already exists')
    image = checked_image(artifacts, variant)
    filename = 'Image' if output_format == 'kernel' else 'boot.img'
    payload = None
    if output_format == 'boot.img':
        require(base is not None, 'boot.img output needs the pinned original boot image')
        original, _ = load_base(base)
        payload = repack_bytes(original, image.read_bytes())
        parse_boot(payload)
    destination.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='devon-output-', dir=destination.parent) as temporary:
        work = Path(temporary) / 'files'
        work.mkdir()
        target = work / filename
        if payload is None:
            shutil.copyfile(image, target)
            require(digest(target) == digest(image), 'Raw kernel copy changed')
        else:
            target.write_bytes(payload)
            require(load_base(base)[0] == original, 'Original boot changed during repacking')
        checksum = filename + '.sha256'
        (work / checksum).write_text(digest(target) + '  ' + filename + '\n')
        require(sorted(p.name for p in work.iterdir()) == sorted([filename, checksum]), 'Unexpected artifact contents')
        work.rename(destination)
    print('Prepared ' + filename + ' and ' + checksum)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--variant', choices=['bakasu', 'susfs'], required=True)
    parser.add_argument('--format', choices=['kernel', 'boot.img'], required=True)
    parser.add_argument('--artifacts', type=Path)
    parser.add_argument('--base', type=Path)
    parser.add_argument('--output-directory', type=Path, default=Path('dist'))
    args = parser.parse_args()
    artifacts = args.artifacts
    if artifacts is None:
        if args.variant == 'bakasu':
            artifacts = Path('artifacts')
        else:
            results = list(Path('artifacts/susfs-features-test').glob('*/result.json'))
            require(len(results) == 1, 'Expected one unambiguous SUSFS build result')
            artifacts = results[0].parent
    package(artifacts.resolve(strict=True), args.variant, args.format,
            args.output_directory.resolve(), args.base)
