#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0
"""Independent Git tree comparison and tampered compiler rejection."""
import io
import json
import os
from pathlib import Path
import subprocess
import tarfile
import tempfile
from toolchain_integrity import content_tree, verify_directory, prepare_toolchain, sha


def rejected(action, expected):
    try:
        action()
    except (ValueError, FileNotFoundError) as error:
        assert expected in str(error), str(error)
    else:
        raise AssertionError('Expected rejection: ' + expected)


with tempfile.TemporaryDirectory() as temp:
    root = Path(temp)
    source = root / 'source'
    (source / 'bin').mkdir(parents=True)
    program = source / 'bin/clang'
    program.write_text('#!/bin/sh\necho content-verification-fixture\n')
    program.chmod(0o755)
    (source / 'library').write_text('original library')
    (source / 'bin/clang++').symlink_to('clang')
    pins = {'files': {'bin/clang': sha(program)}}
    # Compare our traversal, framing and executable/symlink modes to Git itself.
    for algorithm in ['sha1', 'sha256']:
        repo = root / algorithm
        subprocess.run(['git', 'init', '-q', '--object-format=' + algorithm, str(repo)], check=True)
        import shutil
        shutil.copytree(source, repo, dirs_exist_ok=True, symlinks=True)
        subprocess.run(['git', 'add', '.'], cwd=repo, check=True)
        expected = subprocess.check_output(['git', 'write-tree'], cwd=repo, text=True).strip()
        assert content_tree(source, algorithm) == expected
    pins['content_tree_sha256'] = content_tree(source)
    pins['git_tree'] = content_tree(source, 'sha1')
    verify_directory(source, pins)
    os.utime(source / 'library', (10, 10))
    verify_directory(source, pins)
    (source / 'library').write_text('tampered library')
    rejected(lambda: verify_directory(source, pins), 'content tree')
    (source / 'library').write_text('original library')
    program.chmod(0o644)
    rejected(lambda: verify_directory(source, pins), 'content tree')
    program.chmod(0o755)
    # Different container checksum, same pinned contents, is acceptable only
    # after the complete content and independent executable gates both pass.
    archive = root / 'fixture.tar.gz'
    with tarfile.open(archive, 'w:gz') as bundle:
        for path in source.iterdir():
            bundle.add(path, arcname=path.name)
    tool = dict(pins, url=archive.as_uri(), archive_sha256='0' * 64)
    assert prepare_toolchain(root / 'cache/installed', {'toolchain': tool}) == 'content-verification-fixture'
    tool['files'] = {'bin/clang': '0' * 64}
    rejected(lambda: prepare_toolchain(root / 'other/installed', {'toolchain': tool}), 'fingerprint')
    # Legacy fixtures with only an archive pin remain strict about transport.
    rejected(lambda: prepare_toolchain(root / 'legacy/installed', {'toolchain': {
        'url': archive.as_uri(), 'archive_sha256': '0' * 64, 'files': pins['files']}}), 'archive checksum')
print('PASS: Git SHA1/SHA256 agreement; timestamp independence; library/mode/executable/transport tampering rejected')
