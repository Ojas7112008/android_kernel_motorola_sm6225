# SPDX-License-Identifier: GPL-2.0
"""Immutable clean-kernel and archived legacy negative-control inputs."""
import hashlib
import json
import subprocess
from pathlib import Path


def kernel_source(root, name, legacy=False):
    if legacy:
        directory = root / 'scripts/susfs/fixtures/legacy-kernel'
        pins = json.loads((directory / 'manifest.json').read_text())
        data = (directory / name).read_bytes()
        if hashlib.sha256(data).hexdigest() != pins['files'][name]:
            raise ValueError('Legacy regression fixture changed: ' + name)
        return data.decode()
    pins = json.loads((root / 'scripts/susfs/build-pins.json').read_text())
    return subprocess.check_output(['git', 'show', pins['kernel_base'] + ':' + name], cwd=root).decode()
