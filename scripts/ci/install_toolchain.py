#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0
"""Install and fingerprint the pinned official Android Clang archive."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import tarfile
import tempfile
import urllib.request

def sha(path):
    h = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            h.update(block)
    return h.hexdigest()

def run(args):
    return subprocess.check_output(args, text=True).strip()

def prepare_toolchain(directory, pins):
    tool = pins['toolchain']
    if not directory.exists():
        directory.parent.mkdir(parents=True, exist_ok=True)
        archive = directory.parent / 'android-16-r547379.tar.gz'
        if not archive.exists():
            partial = archive.with_suffix('.download')
            print('Downloading pinned Android Clang archive...', flush=True)
            urllib.request.urlretrieve(tool['url'], partial)
            if sha(partial) != tool['archive_sha256']:
                raise ValueError('Downloaded toolchain checksum mismatch')
            partial.rename(archive)
        if sha(archive) != tool['archive_sha256']:
            raise ValueError('Cached toolchain archive checksum mismatch')
        with tempfile.TemporaryDirectory(prefix='devon-clang-', dir=directory.parent) as temp:
            destination = Path(temp)
            with tarfile.open(archive, 'r:gz') as bundle:
                # The exact verified official archive uses relative paths.
                for member in bundle.getmembers():
                    if Path(member.name).is_absolute() or '..' in Path(member.name).parts:
                        raise ValueError('Unexpected archive path')
                    if member.issym() or member.islnk():
                        parent = (destination / member.name).parent if member.issym() else destination
                        (parent / member.linkname).resolve().relative_to(destination.resolve())
                bundle.extractall(destination, filter='data') if hasattr(tarfile, 'data_filter') else bundle.extractall(destination)
            destination.rename(directory)
    for name, expected in tool['files'].items():
        if sha(directory / name) != expected:
            raise ValueError(f'Toolchain fingerprint mismatch: {name}')
    return run([str(directory / 'bin/clang'), '--version'])

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--directory', type=Path, default=Path(__file__).resolve().parents[3] / '.toolchains/android-16-r547379')
    args = parser.parse_args()
    pins = json.loads(Path(__file__).with_name('toolchain-pins.json').read_text())
    print(prepare_toolchain(args.directory.resolve(), pins))
