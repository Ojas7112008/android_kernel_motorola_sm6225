#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0
"""Packaging contracts; fixtures are temporary and never device-test images."""
import argparse
import copy
import hashlib
import json
from pathlib import Path
import struct
import tempfile
import unittest

import axion_boot_layout as layout
from fetch_boot_base import fetch
from package_output import package

BASE = KERNEL = COMPRESSED = None


def fixture(directory, variant, kernel=None):
    directory.mkdir()
    if kernel is None:
        kernel = bytearray(2 * 1024 * 1024 + 7)
        kernel[56:60] = b'ARM\x64'
        struct.pack_into('<Q', kernel, 16, len(kernel) + 4096)
    (directory / 'Image').write_bytes(kernel)
    config = ''.join('CONFIG_' + key + '=y\n' for key in ['ARM64', 'KSU', 'LTO_CLANG', 'CFI_CLANG', 'MODVERSIONS'])
    config += ('CONFIG_KSU_SUSFS=y\n' if variant == 'susfs' else 'CONFIG_KSU_MANUAL_HOOK=y\n')
    (directory / 'kernel.config').write_text(config)
    sha = lambda name: hashlib.sha256((directory / name).read_bytes()).hexdigest()
    if variant == 'susfs':
        features = ['SUS_PATH', 'SUS_MOUNT']
        (directory / 'result.json').write_text(json.dumps({'status': 'compiled-and-linked', 'profile': 'all', 'features': features}))
        (directory / 'build-pins.json').write_text(json.dumps({'profiles': {'all': features}}))
        (directory / 'SHA256SUMS.json').write_text(json.dumps({name: sha(name) for name in ['Image', 'kernel.config']}))
    else:
        (directory / 'build-result.json').write_text(json.dumps({'linked_bakasu_symbols': 10, 'kernel_modules': 2,
            'artifacts': {name: {'sha256': sha(name), 'bytes': (directory / name).stat().st_size} for name in ['Image', 'kernel.config']}}))
    return directory


class Packaging(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)

    def tearDown(self):
        self.temp.cleanup()

    def assert_two_files(self, directory, name):
        self.assertEqual(sorted(p.name for p in directory.iterdir()), sorted([name, name + '.sha256']))
        self.assertEqual((directory / (name + '.sha256')).read_text(), hashlib.sha256((directory / name).read_bytes()).hexdigest() + '  ' + name + '\n')

    def test_raw_both_variants_are_byte_identical_and_need_no_boot_base(self):
        for variant in ['bakasu', 'susfs']:
            source = fixture(self.root / variant, variant)
            out = self.root / (variant + '-output')
            package(source, variant, 'kernel', out, self.root / 'unavailable-base.img')
            self.assert_two_files(out, 'Image')
            self.assertEqual((out / 'Image').read_bytes(), (source / 'Image').read_bytes())

    def test_corrupt_image_rejected_without_output(self):
        source = fixture(self.root / 'build', 'susfs')
        image = source / 'Image'
        data = bytearray(image.read_bytes());data[-1] ^= 1;image.write_bytes(data)
        with self.assertRaisesRegex(ValueError, 'Kernel checksum'):
            package(source, 'susfs', 'kernel', self.root / 'out')
        self.assertFalse((self.root / 'out').exists())

    def test_wrong_variant_rejected(self):
        source = fixture(self.root / 'build', 'susfs')
        with self.assertRaises(FileNotFoundError):
            package(source, 'bakasu', 'kernel', self.root / 'out')

    def test_failed_build_rejected(self):
        source = fixture(self.root / 'build', 'susfs')
        p = source / 'result.json';d = json.loads(p.read_text());d['status'] = 'failed';p.write_text(json.dumps(d))
        with self.assertRaisesRegex(ValueError, 'successful'):
            package(source, 'susfs', 'kernel', self.root / 'out')

    def test_config_tampering_rejected(self):
        source = fixture(self.root / 'build', 'bakasu')
        (source / 'kernel.config').write_text('CONFIG_KSU=y\n')
        with self.assertRaisesRegex(ValueError, 'Configuration checksum'):
            package(source, 'bakasu', 'kernel', self.root / 'out')

    def test_unknown_format_rejected(self):
        source = fixture(self.root / 'build', 'bakasu')
        with self.assertRaisesRegex(ValueError, 'Unknown output'):
            package(source, 'bakasu', 'other', self.root / 'out')

    def test_no_overwrite_of_existing_output(self):
        source = fixture(self.root / 'build', 'bakasu')
        out = self.root / 'out';out.mkdir();(out / 'keep').write_text('keep')
        with self.assertRaisesRegex(ValueError, 'already exists'):
            package(source, 'bakasu', 'kernel', out)
        self.assertEqual((out / 'keep').read_text(), 'keep')

    def test_boot_requires_pinned_base(self):
        source = fixture(self.root / 'build', 'bakasu')
        with self.assertRaisesRegex(ValueError, 'needs the pinned original'):
            package(source, 'bakasu', 'boot.img', self.root / 'out')
        wrong = self.root / 'wrong.img';wrong.write_bytes(b'wrong base')
        with self.assertRaisesRegex(ValueError, 'Base SHA-256'):
            package(source, 'bakasu', 'boot.img', self.root / 'out', wrong)
        self.assertFalse((self.root / 'out').exists())

    def test_real_base_boot_output_preserves_ramdisk_header_and_avb(self):
        if BASE is None: self.skipTest('real supplied base not requested')
        original, old = layout.load_base(BASE)
        kernel = (KERNEL.read_bytes() if KERNEL else original[layout.PAGE:layout.PAGE + old['kernel_size']])
        for variant in ['bakasu', 'susfs']:
            source = fixture(self.root / variant, variant, kernel)
            out = self.root / (variant + '-output')
            package(source, variant, 'boot.img', out, BASE)
            self.assert_two_files(out, 'boot.img')
            data = (out / 'boot.img').read_bytes();new = layout.parse_boot(data)
            self.assertEqual(data[layout.PAGE:layout.PAGE + len(kernel)], kernel)
            self.assertEqual(data[new['ramdisk_start']:new['ramdisk_end']], original[old['ramdisk_start']:old['ramdisk_end']])
            self.assertEqual(data[12:layout.PAGE], original[12:layout.PAGE])
            self.assertEqual(data[-64:-52], original[-64:-52])
        self.assertEqual(hashlib.sha256(BASE.read_bytes()).hexdigest(), layout.BASE_SHA256)

    def test_resource_download_sha_and_cached_base_validation(self):
        if BASE is None or COMPRESSED is None: self.skipTest('local original resource not requested')
        pins = json.loads(Path(__file__).with_name('boot-base.json').read_text())
        pins['url'] = COMPRESSED.as_uri()
        path = fetch(self.root / 'good-cache', pins)
        self.assertEqual(hashlib.sha256(path.read_bytes()).hexdigest(), layout.BASE_SHA256)
        self.assertEqual(fetch(self.root / 'good-cache', pins), path)
        bad = copy.deepcopy(pins);bad['archive_sha256'] = '0' * 64
        with self.assertRaisesRegex(ValueError, 'archive SHA256'):
            fetch(self.root / 'bad-cache', bad)
        path.write_bytes(b'corrupt cache')
        with self.assertRaisesRegex(ValueError, 'Base SHA-256'):
            fetch(self.root / 'good-cache', pins)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--base', type=Path)
    parser.add_argument('--kernel', type=Path)
    parser.add_argument('--compressed', type=Path)
    args, rest = parser.parse_known_args()
    BASE = args.base.resolve() if args.base else None
    KERNEL = args.kernel.resolve() if args.kernel else None
    COMPRESSED = args.compressed.resolve() if args.compressed else None
    unittest.main(argv=[__file__, *rest])
