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
    assert data.get('on', data.get(True)) == {'workflow_dispatch': None}
    assert data['permissions'] == {'contents': 'read'}
    steps = data['jobs']['compile']['steps']
    assert steps[0]['uses'] == 'actions/checkout@11bd71901bbe5b1630ceea73d27597364c9af683'
    assert steps[0]['with']['ref'] == branch and steps[0]['with']['fetch-depth'] == 0
    assert steps[0]['with']['persist-credentials'] is False
    assert steps[-1]['uses'] == 'actions/upload-artifact@ea165f8d65b6e75b540449e92b4886f43607fa02'
    assert steps[-1]['if'] == 'always()'
    assert '--profile all' in '\n'.join(s.get('run', '') for s in steps) if variant == 'susfs' else steps[0]['with']['submodules'] == 'recursive'
print('PASS: two pinned, manual-only kernel workflows with read-only permissions')
