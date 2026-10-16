# SPDX-License-Identifier: GPL-2.0
"""Content-pinned compiler installation, independent of archive timestamps."""
import hashlib
import json
import os
from pathlib import Path
import stat
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


def content_tree(directory, algorithm='sha256'):
    # Git's object framing/order/modes; SHA256 uses 32-byte child object IDs.
    def object_hash(kind, data):
        return hashlib.new(algorithm, kind + b' ' + str(len(data)).encode() + b'\0' + data).digest()
    entries = []
    for path in directory.iterdir():
        mode = path.lstat().st_mode
        if stat.S_ISLNK(mode):
            kind = b'120000'
            digest = object_hash(b'blob', os.fsencode(os.readlink(path)))
        elif stat.S_ISDIR(mode):
            kind = b'40000'
            digest = bytes.fromhex(content_tree(path, algorithm))
        elif stat.S_ISREG(mode):
            kind = b'100755' if mode & 0o111 else b'100644'
            h = hashlib.new(algorithm)
            h.update(b'blob ' + str(path.stat().st_size).encode() + b'\0')
            with path.open('rb') as stream:
                for block in iter(lambda: stream.read(1024 * 1024), b''):
                    h.update(block)
            digest = h.digest()
        else:
            raise ValueError('Unexpected compiler entry type: ' + str(path))
        name = os.fsencode(path.name)
        entries.append((name + (b'/' if kind == b'40000' else b''), kind + b' ' + name + b'\0' + digest))
    return object_hash(b'tree', b''.join(data for _, data in sorted(entries))).hex()


def verify_directory(directory, tool):
    for name, expected in tool['files'].items():
        if sha(directory / name) != expected:
            raise ValueError('Toolchain fingerprint mismatch: ' + name)
    if 'content_tree_sha256' in tool:
        if content_tree(directory) != tool['content_tree_sha256']:
            raise ValueError('Toolchain content tree SHA256 mismatch')
        if content_tree(directory, 'sha1') != tool['git_tree']:
            raise ValueError('Toolchain official Git tree mismatch')


def prepare_toolchain(directory, pins):
    tool = pins['toolchain']
    if not directory.exists():
        directory.parent.mkdir(parents=True, exist_ok=True)
        archive = directory.parent / 'android-16-r547379.tar.gz'
        if not archive.exists():
            partial = archive.with_suffix('.download')
            print('Downloading pinned Android Clang archive...', flush=True)
            urllib.request.urlretrieve(tool['url'], partial)
            actual = sha(partial)
            # Gitiles rewrites tar mtimes. Official inputs therefore require
            # exact whole-tree and executable pins before any tool is run.
            # Small test fixtures without a tree pin still require archive SHA.
            if 'content_tree_sha256' not in tool and actual != tool['archive_sha256']:
                raise ValueError('Downloaded toolchain archive checksum mismatch')
            partial.rename(archive)
        actual = sha(archive)
        if 'content_tree_sha256' not in tool and actual != tool['archive_sha256']:
            raise ValueError('Cached toolchain archive checksum mismatch')
        with tempfile.TemporaryDirectory(prefix='devon-clang-', dir=directory.parent) as temp:
            destination = Path(temp)
            with tarfile.open(archive, 'r:gz') as bundle:
                for member in bundle.getmembers():
                    if Path(member.name).is_absolute() or '..' in Path(member.name).parts:
                        raise ValueError('Unexpected archive path')
                    if not (member.isfile() or member.isdir() or member.issym() or member.islnk()):
                        raise ValueError('Unexpected archive entry type')
                    if member.issym() or member.islnk():
                        parent = (destination / member.name).parent if member.issym() else destination
                        (parent / member.linkname).resolve().relative_to(destination.resolve())
                bundle.extractall(destination, filter='data') if hasattr(tarfile, 'data_filter') else bundle.extractall(destination)
            verify_directory(destination, tool)
            destination.rename(directory)
        (directory.parent / 'android-16-r547379.archive.json').write_text(json.dumps({
            'url': tool['url'], 'archive_sha256_observed': actual,
            'git_tree': tool.get('git_tree'), 'content_tree_sha256': tool.get('content_tree_sha256'),
            'validation': 'Every extracted file, mode and symlink verified before compiler execution'
        }, indent=2) + '\n')
    else:
        verify_directory(directory, tool)
    return subprocess.check_output([str(directory / 'bin/clang'), '--version'], text=True).strip()
