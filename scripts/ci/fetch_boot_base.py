#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0
"""Download only the pinned original Axion resource for boot.img output."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile
import urllib.request
from axion_boot_layout import BASE_SHA256, load_base

PINS_FILE = Path(__file__).with_name('boot-base.json')


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def fetch(directory, pins):
    if pins['image_sha256'] != BASE_SHA256:
        raise ValueError('Boot resource pin differs from the reviewed repacker')
    image = directory / 'boot.img'
    if image.exists():
        load_base(image)
        return image
    directory.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='boot-base-', dir=directory) as temp:
        work = Path(temp)
        archive = work / 'base.img.zst'
        print('Downloading the checksum-pinned original Axion boot image', flush=True)
        with urllib.request.urlopen(pins['url'], timeout=120) as response, archive.open('xb') as output:
            remaining = pins['archive_bytes']
            while remaining:
                block = response.read(min(1024 * 1024, remaining))
                if not block:
                    raise ValueError('Truncated original boot resource')
                output.write(block)
                remaining -= len(block)
            if response.read(1):
                raise ValueError('Oversized original boot resource')
        if sha(archive) != pins['archive_sha256']:
            raise ValueError('Original boot archive SHA256 mismatch')
        candidate = work / 'boot.img'
        with candidate.open('xb') as output:
            subprocess.run(['zstd', '-q', '-d', '-c', str(archive)], stdout=output, check=True)
        if candidate.stat().st_size != pins['image_bytes'] or sha(candidate) != pins['image_sha256']:
            raise ValueError('Original boot image SHA256/size mismatch')
        load_base(candidate)
        # Atomic creation, never replace a pre-existing cached base.
        import os
        os.link(candidate, image)
    return image


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--directory', type=Path, default=Path('.backport-context/build/axion-boot-base'))
    args = parser.parse_args()
    print(fetch(args.directory.resolve(), json.loads(PINS_FILE.read_text())))
