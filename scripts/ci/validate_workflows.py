#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0
"""Check both manual-only workflow contracts."""
from pathlib import Path
import yaml
root = Path(__file__).resolve().parents[2]
expected = {'bakasu': 'lineage-23.2-bakasu', 'susfs': 'lineage-23.2'}
assert sorted(p.name for p in (root / '.github/workflows').glob('*.yml')) == ['build-devon-bakasu.yml', 'build-devon-susfs.yml']
for variant, branch in expected.items():
    data = yaml.safe_load((root / ('.github/workflows/build-devon-' + variant + '.yml')).read_text())
    events = data.get('on', data.get(True))
    assert set(events) == {'workflow_dispatch'}
    choice = events['workflow_dispatch']['inputs']['output_format']
    assert choice['type'] == 'choice' and choice['required'] is True
    assert choice['default'] == 'kernel' and choice['options'] == ['kernel', 'boot.img']
    assert data['permissions'] == {'contents': 'read'}
    steps = data['jobs']['compile']['steps']
    assert steps[0]['uses'] == 'actions/checkout@11bd71901bbe5b1630ceea73d27597364c9af683'
    assert steps[0]['with']['ref'] == branch and steps[0]['with']['fetch-depth'] == 0
    assert steps[0]['with']['persist-credentials'] is False
    assert steps[-1]['uses'] == 'actions/upload-artifact@ea165f8d65b6e75b540449e92b4886f43607fa02'
    assert steps[-1].get('if', 'success()') == 'success()'
    assert steps[-1]['with']['path'] == 'dist/'
    assert steps[-1]['with']['if-no-files-found'] == 'error'
    assert sum('upload-artifact@' in s.get('uses', '') for s in steps) == 1
    assert any(s.get('if') == "inputs.output_format == 'boot.img'" for s in steps)
    assert any('package_output.py --variant ' + variant in s.get('run', '') for s in steps)
    assert '--profile all' in '\n'.join(s.get('run', '') for s in steps) if variant == 'susfs' else steps[0]['with']['submodules'] == 'recursive'
print('PASS: two manual output choices, success-only two-file uploads and pinned read-only workflows')
