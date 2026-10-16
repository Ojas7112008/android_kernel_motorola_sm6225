#!/usr/bin/env python3
"""Exercise limited-build refusals and real Kbuild preflight, without a kernel build."""
import argparse
import copy
import io
import json
import os
from pathlib import Path
import struct
import subprocess
import tarfile
import tempfile
import yaml

from build_test import ROOT, PINS_FILE, FEATURE_OBJECTS, sha, verify_config, profile_config, check_source, check_dependency, prepare_toolchain, validate_image


def must_reject(call, message):
    try:
        call()
    except (ValueError, RuntimeError, subprocess.CalledProcessError) as error:
        assert message in str(error), (message, str(error))
    else:
        raise AssertionError('Expected refusal: ' + message)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('artifacts', type=Path, help='An actual --prepare evidence directory')
    parser.add_argument('--profile', default='core')
    args = parser.parse_args()
    out = ROOT / '.backport-context/audit/all-features/build-preparation' / args.profile
    out.mkdir(parents=True, exist_ok=True)
    pins = json.loads(PINS_FILE.read_text())
    config = (args.artifacts / 'kernel.config').read_text()
    profile = args.profile
    required = profile_config(pins, profile)
    verify_config(config, pins, profile)
    rejected = []
    for key in required:
        if key.startswith('KSU_SUSFS_') and key != 'KSU_SUSFS_ENABLE_LOG':
            bad = (config.replace('# CONFIG_' + key + ' is not set', 'CONFIG_' + key + '=y') if required[key] == 'n'
                   else config.replace('CONFIG_' + key + '=y', '# CONFIG_' + key + ' is not set'))
            assert bad != config
            must_reject(lambda: verify_config(bad, pins, profile), key)
            rejected.append(key)
    must_reject(lambda: verify_config(config + '\nCONFIG_KSU_SUSFS_NEW_UNREVIEWED_FEATURE=y\n', pins, profile), 'Unreviewed feature')
    for key, value in [('KSU', 'm'), ('KSU_DEVON_SELINUX_POLICY', 'n'), ('CFI_CLANG', 'n')]:
        bad = config.replace('CONFIG_' + key + '=y', 'CONFIG_' + key + '=' + value)
        must_reject(lambda: verify_config(bad, pins, profile), key)
    for name, features in pins['profiles'].items():
        selected = profile_config(pins, name)
        assert {f for f in FEATURE_OBJECTS if selected['KSU_SUSFS_' + f] == 'y'} == set(features)
    bad = copy.deepcopy(pins)
    bad['kernel_files']['fs/susfs.c'] = '0' * 64
    must_reject(lambda: check_source(bad), 'fs/susfs.c')
    bad = copy.deepcopy(pins)
    del bad['kernel_files']['fs/Makefile']
    must_reject(lambda: check_source(bad), 'Unreviewed kernel input changed: fs/Makefile')
    repo = ROOT / pins['dependency_directory']
    check_dependency(repo, pins)
    bad = copy.deepcopy(pins)
    bad['provider_files']['kernel/core/init.c'] = '0' * 64
    must_reject(lambda: check_dependency(repo, bad), 'kernel/core/init.c')
    bad = copy.deepcopy(pins)
    bad['bakasu_commit'] = '0' * 40
    must_reject(lambda: check_dependency(repo, bad), 'HEAD')
    bad = copy.deepcopy(pins)
    bad['toolchain']['files']['bin/clang'] = '0' * 64
    must_reject(lambda: prepare_toolchain(ROOT.parent / '.toolchains/android-16-r547379', bad), 'fingerprint')
    with tempfile.TemporaryDirectory(prefix='devon-core-build-test-') as temp:
        directory = Path(temp)
        # Exercise the CI download/extraction branch using an isolated, tiny
        # file:// fixture. It is never presented as an actual compiler.
        archive = directory / 'source.tar.gz'
        program = b'#!/bin/sh\necho toolchain-extraction-fixture\n'
        with tarfile.open(archive, 'w:gz') as bundle:
            member = tarfile.TarInfo('bin/clang')
            member.size = len(program)
            member.mode = 0o755
            bundle.addfile(member, io.BytesIO(program))
        fixture_pins = {'toolchain': {'url': archive.as_uri(), 'archive_sha256': sha(archive),
                                     'files': {'bin/clang': __import__('hashlib').sha256(program).hexdigest()}}}
        assert prepare_toolchain(directory / 'installed', fixture_pins) == 'toolchain-extraction-fixture'
        fixture_pins['toolchain']['archive_sha256'] = '0' * 64
        must_reject(lambda: prepare_toolchain(directory / 'other-install', fixture_pins), 'archive checksum')
        image = directory / 'header-fixture'
        header = bytearray(64)
        header[56:60] = b'ARM\x64'
        struct.pack_into('<Q', header, 16, 4096)
        image.write_bytes(header + bytes(128))
        assert validate_image(image) == {'file_bytes': 192, 'memory_bytes': 4096}
        image.write_bytes(bytes(192))
        must_reject(lambda: validate_image(image), 'header')
        struct.pack_into('<Q', header, 16, 0)
        image.write_bytes(header + bytes(128))
        must_reject(lambda: validate_image(image), 'size')
    # Validate the Image format against an independent, previously successful
    # non-SUSFS build when it is locally available; no output is repackaged.
    existing = ROOT.parent / 'android_kernel_motorola_sm6225-lineage23.2-bakasu/artifacts/Image'
    if existing.exists():
        assert validate_image(existing)['memory_bytes'] > existing.stat().st_size
    env = os.environ.copy()
    env['PATH'] = str(ROOT.parent / '.toolchains/android-16-r547379/bin') + os.pathsep + env['PATH']
    for name, assignment, expected in [
        ('reject-module-ksu', 'CONFIG_KSU=m', 'requires built-in'),
        ('reject-module-ksu-uppercase', 'CONFIG_KSU=M', 'requires built-in'),
        ('reject-no-susfs', 'CONFIG_KSU_SUSFS=n', 'requires the SUSFS integration'),
        ('reject-wrong-kernel', 'PATCHLEVEL=14', 'requires Linux 4.19'),
    ]:
        result = subprocess.run(['make', '--no-print-directory', '-f', str(args.artifacts / 'kbuild-preflight.mk'),
                                 assignment, 'devon_preflight'], cwd=ROOT, env=env, text=True, capture_output=True)
        (out / (name + '.log')).write_text(result.stdout + result.stderr)
        assert result.returncode and expected in result.stdout + result.stderr, name
    builtin = subprocess.run(['make', '--no-print-directory', '-f', str(args.artifacts / 'kbuild-preflight.mk'),
                              'CONFIG_KSU=Y', 'devon_preflight'], cwd=ROOT, env=env, text=True, capture_output=True)
    (out / 'uppercase-builtin-preflight.log').write_text(builtin.stdout + builtin.stderr)
    assert builtin.returncode == 0 and 'KSU_COMPAT_DEVON_SELINUX_POLICY' in builtin.stdout
    workflow = ROOT / '.github/workflows/build-devon-susfs.yml'
    content = workflow.read_text()
    # PyYAML uses YAML 1.1 and reads GitHub's `on` as a boolean key.
    data = yaml.safe_load(content)
    assert data.get('on', data.get(True)) == {'workflow_dispatch': None}
    steps = data['jobs']['compile']['steps']
    assert steps[0]['uses'] == 'actions/checkout@' + pins['actions']['checkout']
    assert steps[-1]['uses'] == 'actions/upload-artifact@' + pins['actions']['upload-artifact']
    assert data['permissions'] == {'contents': 'read'}
    assert any(s.get('run') == 'bash ./build-susfs-test.sh --build --profile all --jobs 4' for s in steps)
    subprocess.run(['bash', '-n', str(ROOT / 'build-susfs-test.sh')], check=True)
    compile((ROOT / 'scripts/susfs/build_test.py').read_text(), 'build_test.py', 'exec')
    files = ['build-susfs-test.sh', 'scripts/susfs/build_test.py', 'scripts/susfs/build-pins.json',
             'scripts/susfs/test_build_preparation.py', '.github/workflows/build-devon-susfs.yml']
    result = {'scope': __doc__, 'status': 'passed', 'profile': profile, 'feature_drift_refusals': rejected,
              'other_refusals': 'Unknown feature, module KSU, missing policy/CFI, source/provider/HEAD/compiler drift, corrupt archive, invalid Image header/size',
              'toolchain_install': 'file:// fixture download, checksum, extraction and executable permissions passed; official archive separately verified',
              'native_kbuild_rejections': 4, 'uppercase_builtin': 'accepted; lowercase/uppercase module values rejected',
              'workflow': 'manual only; exact action refs; read-only repository permission',
              'positive_config': str(args.artifacts / 'kernel.config'),
              'inputs': {p: sha(ROOT / p) for p in files}}
    (out / 'preparation-tests.json').write_text(json.dumps(result, indent=2) + '\n')
    print('PASS: real prepared config, feature/source/compiler/Image refusals, four native Kbuild rejections, uppercase built-in and manual workflow')


if __name__ == '__main__':
    main()
