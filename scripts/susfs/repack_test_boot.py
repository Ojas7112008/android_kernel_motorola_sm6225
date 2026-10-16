#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0
"""Create a separate Axion test boot image from a reviewed feature-profile build.

No device commands, flashing, ramdisk modification or verification-flag changes.
"""
import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile

import axion_boot_layout as layout
from build_test import ROOT, PINS_FILE, sha, check_source, check_dependency, verify_config, validate_image


def require(condition, message):
    if not condition:
        raise ValueError(message)


def main():
    pins = json.loads(PINS_FILE.read_text())
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--base', required=True, type=Path)
    parser.add_argument('--artifacts', required=True, type=Path)
    parser.add_argument('--profile', choices=list(pins['profiles']), default='all')
    args = parser.parse_args()
    base_path = args.base.resolve(strict=True)
    artifacts = args.artifacts.resolve(strict=True)
    branch = check_source(pins)
    check_dependency(ROOT / pins['dependency_directory'], pins)
    record = json.loads((artifacts / 'result.json').read_text())
    require(record['status'] == 'compiled-and-linked' and record['profile'] == args.profile,
            'Expected a successful build of the selected profile')
    require(record['bakasu_commit'] == pins['bakasu_commit'] and
            record['features'] == pins['profiles'][args.profile], 'Source/feature selection differs')
    require(json.loads((artifacts / 'build-pins.json').read_text()) == pins,
            'Build source pins differ from the current reviewed inputs')
    require((artifacts / 'build-driver.py').read_bytes() == (ROOT / 'scripts/susfs/build_test.py').read_bytes(),
            'Build driver changed since compilation')
    for name, expected in json.loads((artifacts / 'SHA256SUMS.json').read_text()).items():
        path = (artifacts / name).resolve(strict=True)
        require(path.is_relative_to(artifacts) and sha(path) == expected, 'Artifact mismatch: ' + name)
    config = verify_config((artifacts / 'kernel.config').read_text(), pins, args.profile)
    require(config.get('CFI_PERMISSIVE', 'n') == 'n', 'CFI must remain enforcing')
    validate_image(artifacts / 'Image')
    base, original = layout.load_base(base_path)
    kernel = (artifacts / 'Image').read_bytes()
    output = layout.repack_bytes(base, kernel)
    updated = layout.parse_boot(output)
    stamp = datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%SZ')
    parent = ROOT / 'artifacts/susfs-features-test/boot-tests'
    parent.mkdir(parents=True, exist_ok=True)
    run = Path(tempfile.mkdtemp(prefix=stamp + '-', dir=parent))
    filename = ('boot-bakasu-susfs-test.img' if args.profile == 'all' else
                'boot-bakasu-susfs-' + args.profile + '-test.img')
    destination = run / filename
    with destination.open('xb') as stream:
        stream.write(output)
        stream.flush()
        os.fsync(stream.fileno())
    require(sha(base_path) == layout.BASE_SHA256, 'Original changed during repacking')
    require(sha(destination) == hashlib.sha256(output).hexdigest(), 'Output write mismatch')
    result = {
        'status': 'repacked-and-structurally-validated', 'base': str(base_path),
        'base_sha256': layout.BASE_SHA256, 'output': str(destination),
        'output_sha256': sha(destination), 'partition_bytes': len(output),
        'header_version': 3, 'kernel_sha256': sha(artifacts / 'Image'),
        'kernel_bytes': len(kernel), 'original_kernel_bytes': original['kernel_size'],
        'ramdisk_bytes': original['ramdisk_size'],
        'ramdisk_sha256': hashlib.sha256(base[original['ramdisk_start']:original['ramdisk_end']]).hexdigest(),
        'ramdisk_and_padding_preserved': True, 'boot_settings_preserved': True,
        'avb_algorithm': 'NONE, unchanged', 'avb_flags_preserved': True,
        'avb_boot_hash_verified': True, 'vbmeta_bytes': updated['vbmeta_size'],
        'bakasu_commit': record['bakasu_commit'], 'susfs_features': record['features'],
        'profile': args.profile, 'separate_modules_archive_injected': False,
        'kernel_artifacts': str(artifacts), 'branch': branch,
        'boot_attempt': 'not attempted',
        'scope': 'Byte/layout/hash and build provenance checks; boot/runtime safety unproven',
        'tools': {name: sha(ROOT / 'scripts/susfs' / name) for name in
                  ['repack_test_boot.py', 'axion_boot_layout.py', 'build_test.py']},
    }
    (run / 'manifest.json').write_text(json.dumps(result, indent=2) + '\n')
    (run / 'SHA256SUMS.txt').write_text(result['output_sha256'] + '  ' + destination.name + '\n')
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
