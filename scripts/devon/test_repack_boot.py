#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0
"""Exercise boot-layout failure handling and resizing using the supplied base.

Synthetic resized kernels test packaging only; they are never published as images.
The integration check separately uses the real previously compiled kernel.
"""
import argparse
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest

import repack_boot as repack


class RepackTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.base, cls.layout = repack.load_base(BASE)
        cls.original_kernel = cls.base[repack.PAGE:repack.PAGE + cls.layout['kernel_size']]

    def test_original_kernel_roundtrip_is_byte_identical(self):
        self.assertEqual(repack.repack_bytes(self.base, self.original_kernel), self.base)

    def test_smaller_kernel_moves_ramdisk_and_footer(self):
        kernel = self.original_kernel[:2 * 1024 * 1024 + 7]
        result = repack.repack_bytes(self.base, kernel)
        layout = repack.parse_boot(result)
        self.assertLess(layout['ramdisk_start'], self.layout['ramdisk_start'])
        self.assertEqual(result[layout['ramdisk_start']:layout['ramdisk_end']],
                         self.base[self.layout['ramdisk_start']:self.layout['ramdisk_end']])

    def test_larger_kernel_moves_ramdisk_and_footer(self):
        kernel = self.original_kernel + b'\0' * (3 * 1024 * 1024 + 11)
        result = repack.repack_bytes(self.base, kernel)
        self.assertGreater(repack.parse_boot(result)['vbmeta_offset'], self.layout['vbmeta_offset'])

    def test_real_compiled_image_payload(self):
        kernel = (ARTIFACTS / 'Image').read_bytes()
        result = repack.repack_bytes(self.base, kernel)
        self.assertEqual(result[repack.PAGE:repack.PAGE + len(kernel)], kernel)

    def test_kernel_partition_overflow_rejected(self):
        kernel = self.original_kernel + b'\0' * (70 * 1024 * 1024)
        with self.assertRaisesRegex(ValueError, 'does not fit'):
            repack.repack_bytes(self.base, kernel)

    def test_compressed_or_invalid_kernel_rejected(self):
        with self.assertRaisesRegex(ValueError, 'raw ARM64'):
            repack.repack_bytes(self.base, b'\0' * (2 * 1024 * 1024))

    def test_corrupted_kernel_is_detected_by_avb(self):
        data = bytearray(self.base)
        data[repack.PAGE + 128] ^= 1
        with self.assertRaisesRegex(ValueError, 'digest does not match'):
            repack.parse_boot(data)

    def test_corrupted_ramdisk_is_detected_by_avb(self):
        data = bytearray(self.base)
        data[self.layout['ramdisk_start'] + 128] ^= 1
        with self.assertRaisesRegex(ValueError, 'digest does not match'):
            repack.parse_boot(data)

    def test_signed_vbmeta_is_never_downgraded(self):
        data = bytearray(self.base)
        struct.pack_into('>I', data, self.layout['vbmeta_offset'] + 28, 1)
        with self.assertRaisesRegex(ValueError, 'Signed AVB is unsupported'):
            repack.parse_boot(data)

    def test_other_boot_header_rejected(self):
        data = bytearray(self.base)
        struct.pack_into('<I', data, 40, 4)
        with self.assertRaisesRegex(ValueError, 'header v3'):
            repack.parse_boot(data)

    def test_bad_footer_offset_rejected(self):
        data = bytearray(self.base)
        struct.pack_into('>Q', data, len(data) - 44, len(data) + 4096)
        with self.assertRaisesRegex(ValueError, 'layout'):
            repack.parse_boot(data)

    def test_truncated_image_rejected(self):
        with self.assertRaisesRegex(ValueError, '96 MiB'):
            repack.parse_boot(self.base[:-1])

    def test_unknown_base_fingerprint_rejected(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / 'wrong.img'
            path.write_bytes(b'ANDROID!' + b'\0' * 128)
            with self.assertRaisesRegex(ValueError, 'Base SHA-256 differs'):
                repack.load_base(path)

    def test_wrong_bakasu_build_rejected(self):
        with self.assertRaisesRegex(ValueError, 'selected BakaSU commit'):
            repack.validate_artifacts(ARTIFACTS, '0' * 40)

    def test_cli_cannot_overwrite_original_image(self):
        with tempfile.TemporaryDirectory() as folder:
            result = subprocess.run([sys.executable, '-B', str(Path(repack.__file__)),
                '--base', str(BASE), '--kernel-artifacts', str(ARTIFACTS),
                '--bakasu-commit', 'd7260ca1e5c128061122924d0d48754e361e135c',
                '--output', str(BASE), '--report', str(Path(folder) / 'report.json')],
                capture_output=True, text=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn('cannot replace the input', result.stderr)
            self.assertEqual(repack.sha(BASE.read_bytes()), repack.BASE_SHA256)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--base', required=True, type=Path)
    parser.add_argument('--artifacts', type=Path, default=Path('artifacts'))
    args, remaining = parser.parse_known_args()
    BASE, ARTIFACTS = args.base.resolve(), args.artifacts.resolve()
    unittest.main(argv=[sys.argv[0], *remaining])
