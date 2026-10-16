#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0
"""Materialize exact upstream references for source/ABI tests; refuse drift."""
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[2]
PINS = json.loads((ROOT / 'scripts/susfs/build-pins.json').read_text())


def run(args, cwd=ROOT):
    return subprocess.check_output(args, cwd=cwd, text=True).strip()


def prepare(name, url, commit, full=False):
    repo = ROOT / '.backport-context/repos' / name
    if not repo.exists():
        repo.parent.mkdir(parents=True, exist_ok=True)
        if full:
            provider = ROOT / PINS['dependency_directory']
            source = str(provider) if provider.exists() else url
            run(['git', 'clone', '--no-hardlinks', '--no-checkout', source, str(repo)])
            run(['git', 'remote', 'set-url', 'origin', url], repo)
            if subprocess.run(['git', 'cat-file', '-e', commit + '^{commit}'], cwd=repo,
                              stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL).returncode:
                run(['git', 'fetch', 'origin', commit], repo)
        else:
            run(['git', 'init', '--quiet', str(repo)])
            run(['git', 'remote', 'add', 'origin', url], repo)
            run(['git', 'fetch', '--depth=1', 'origin', commit], repo)
        run(['git', 'checkout', '--detach', commit], repo)
    if run(['git', 'rev-parse', 'HEAD'], repo) != commit:
        raise ValueError('Reference HEAD differs: ' + name)
    if run(['git', 'status', '--porcelain'], repo):
        raise ValueError('Reference has local changes: ' + name)
    if full and run(['git', 'rev-parse', '--is-shallow-repository'], repo) != 'false':
        raise ValueError('Full provider history required')
    print(name + ': ' + commit, flush=True)


if __name__ == '__main__':
    prepare('bakasu-sync-susfs', PINS['bakasu_url'], PINS['bakasu_commit'], full=True)
    prepare('susfs', 'https://github.com/ShirkNeko/susfs4ksu.git', PINS['susfs_reference'])
