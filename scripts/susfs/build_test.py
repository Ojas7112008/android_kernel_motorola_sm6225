#!/usr/bin/env python3
"""Prepare or build pinned devon SUSFS experiments; no boot/runtime claim."""
import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import tarfile
import tempfile
import urllib.request

ROOT = Path(__file__).resolve().parents[2]
PINS_FILE = ROOT / 'scripts/susfs/build-pins.json'
FEATURE_OBJECTS = {
    'SUS_PATH': ['susfs_path.o', 'susfs_fuse.o'],
    'SUS_MOUNT': ['susfs_mount.o'],
    'SUS_KSTAT': ['susfs_kstat.o'],
    'SPOOF_UNAME': ['susfs_uname.o'],
    'HIDE_KSU_SUSFS_SYMBOLS': [],
    'SPOOF_CMDLINE_OR_BOOTCONFIG': ['susfs_cmdline.o'],
    'OPEN_REDIRECT': ['susfs_redirect.o'],
    'SUS_MAP': ['susfs_map.o'],
}


def profile_config(pins, profile='core'):
    features = pins['profiles'][profile]
    if len(set(features)) != len(features) or not set(features) <= FEATURE_OBJECTS.keys():
        raise ValueError('Unknown or duplicate feature in reviewed profile')
    expected = dict(pins['config'])
    for feature in FEATURE_OBJECTS:
        expected['KSU_SUSFS_' + feature] = 'y' if feature in features else 'n'
    return expected


def sha(path):
    h = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            h.update(block)
    return h.hexdigest()


def run(args, cwd=ROOT, env=None, log=None, input_text=None):
    if log:
        with log.open('w') as stream:
            result = subprocess.run(args, cwd=cwd, env=env, input=input_text, text=True,
                                    stdout=stream, stderr=subprocess.STDOUT)
        if result.returncode:
            raise RuntimeError(f'Command failed ({result.returncode}); see {log}')
        return log.read_text()
    return subprocess.check_output(args, cwd=cwd, env=env, text=True, input=input_text).strip()


def parse_config(text):
    result = {}
    for line in text.splitlines():
        match = re.fullmatch(r'CONFIG_(\w+)=(.*)', line)
        unset = re.fullmatch(r'# CONFIG_(\w+) is not set', line)
        if match:
            result[match[1]] = match[2]
        elif unset:
            result[unset[1]] = 'n'
    return result


def verify_config(text, pins, profile='core'):
    config = parse_config(text)
    required = profile_config(pins, profile)
    for key, expected in required.items():
        if config.get(key, 'n') != expected:
            raise ValueError(f'Configuration mismatch: CONFIG_{key}: {config.get(key, "n")} != {expected}')
    for key, value in config.items():
        if key.startswith('KSU_SUSFS_') and value in ('y', 'm') and key not in required:
            raise ValueError(f'Unreviewed feature enabled: CONFIG_{key}')
    return config


def check_source(pins):
    branch = run(['git', 'branch', '--show-current'])
    if branch == 'devon-resukisu':
        raise ValueError('Use the SUSFS backport checkout, not the protected working branch')
    run(['git', 'merge-base', '--is-ancestor', pins['kernel_base'], 'HEAD'])
    # A fresh Lineage fork has no legacy devon-resukisu branch. Check the
    # historical anchor if retained locally; never create or update it here.
    anchor = subprocess.run(['git', 'rev-parse', '--verify', 'refs/remotes/origin/devon-resukisu'],
                            cwd=ROOT, text=True, capture_output=True)
    if anchor.returncode == 0 and anchor.stdout.strip() != pins['protected_ref']:
        raise ValueError('Protected reference differs from the handoff')
    for name, expected in pins['kernel_files'].items():
        if sha(ROOT / name) != expected:
            raise ValueError(f'Source differs from the reviewed build inputs: {name}')
    source_roots = ('arch/', 'block/', 'certs/', 'crypto/', 'drivers/', 'firmware/', 'fs/', 'include/',
                    'init/', 'ipc/', 'kernel/', 'lib/', 'mm/', 'net/', 'security/', 'sound/',
                    'techpack/', 'tools/', 'usr/', 'virt/', 'scripts/')
    changes = run(['git', 'diff', pins['kernel_base'], '--name-only']).splitlines()
    changes += run(['git', 'ls-files', '--others', '--exclude-standard']).splitlines()
    for name in changes:
        if name.startswith(('scripts/susfs/', 'scripts/ci/')) or name == 'drivers/kernelsu':
            continue
        if (name.startswith(source_roots) or name in ('Makefile', 'Kconfig', 'Kbuild')) and name not in pins['kernel_files']:
            raise ValueError(f'Unreviewed kernel input changed: {name}')
    if sha(ROOT / pins['provider_patch']) != pins['provider_sha256']:
        raise ValueError('BakaSU provider patch changed')
    return branch


def check_dependency(repo, pins):
    if run(['git', 'rev-parse', 'HEAD'], cwd=repo) != pins['bakasu_commit']:
        raise ValueError('Build dependency HEAD does not match the BakaSU pin')
    if run(['git', 'rev-parse', '--is-shallow-repository'], cwd=repo) != 'false':
        raise ValueError('Full BakaSU history is required for the native version')
    if run(['git', 'diff', '--cached', '--name-only'], cwd=repo):
        raise ValueError('Build dependency has staged changes')
    if run(['git', 'ls-files', '--others', '--exclude-standard'], cwd=repo):
        raise ValueError('Build dependency contains untracked files')
    actual = set(run(['git', 'diff', '--name-only'], cwd=repo).splitlines())
    if actual != set(pins['provider_files']):
        raise ValueError('Build dependency changes do not match the reviewed provider file list')
    for name, expected in pins['provider_files'].items():
        if sha(repo / name) != expected:
            raise ValueError(f'Build dependency provider mismatch: {name}')
    run(['git', 'diff', '--check'], cwd=repo)
    run(['git', 'apply', '--reverse', '--check', str(ROOT / pins['provider_patch'])], cwd=repo)


def prepare_dependency(pins):
    repo = ROOT / pins['dependency_directory']
    if not repo.exists():
        repo.parent.mkdir(parents=True, exist_ok=True)
        reference = ROOT / '.backport-context/repos/bakasu-sync-susfs'
        source = str(reference) if reference.exists() else pins['bakasu_url']
        run(['git', 'clone', '--no-hardlinks', '--no-checkout', source, str(repo)])
        run(['git', 'remote', 'set-url', 'origin', pins['bakasu_url']], cwd=repo)
        if subprocess.run(['git', 'cat-file', '-e', pins['bakasu_commit'] + '^{commit}'], cwd=repo,
                          stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL).returncode:
            run(['git', 'fetch', 'origin', pins['bakasu_commit']], cwd=repo)
        run(['git', 'checkout', '--detach', pins['bakasu_commit']], cwd=repo)
    if not run(['git', 'status', '--porcelain'], cwd=repo):
        if run(['git', 'rev-parse', 'HEAD'], cwd=repo) != pins['bakasu_commit']:
            raise ValueError('Existing clean dependency has the wrong HEAD; it will not be reset')
        patch = str(ROOT / pins['provider_patch'])
        run(['git', 'apply', '--check', '--whitespace=error-all', patch], cwd=repo)
        run(['git', 'apply', '--whitespace=error-all', patch], cwd=repo)
    check_dependency(repo, pins)
    link = ROOT / 'drivers/kernelsu'
    expected = os.path.relpath(repo / 'kernel', link.parent)
    if not link.is_symlink() or os.readlink(link) != expected:
        raise ValueError('drivers/kernelsu must be the reviewed relative symlink')
    return repo


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


def kbuild_preflight(out, artifacts, env, pins, profile='core'):
    # Evaluate the real provider Kbuild and its upstream checks. The only target
    # is a no-op: this does not compile a kernel object or generate an image.
    makefile = f'''srctree := {ROOT}
objtree := {out}
src := drivers/kernelsu
obj := drivers/kernelsu
VERSION := 4
PATCHLEVEL := 19
SUBLEVEL := 325
CC := clang
include {out}/.config
include {ROOT}/scripts/Kbuild.include
include {ROOT}/drivers/kernelsu/Kbuild
$(info DEVON_KSU_OBJECTS=$(kernelsu-objs))
$(info DEVON_KSU_FLAGS=$(ccflags-y))
include {ROOT}/fs/Makefile
$(info DEVON_SUSFS_OBJECTS=$(filter susfs%,$(obj-y)))
.PHONY: devon_preflight
devon_preflight:
\t@:
'''
    (artifacts / 'kbuild-preflight.mk').write_text(makefile)
    stdout = run(['make', '--no-print-directory', '-f', str(artifacts / 'kbuild-preflight.mk'), 'devon_preflight'],
                 env=env, log=artifacts / 'kbuild-preflight.log')
    selected = re.search(r'^DEVON_SUSFS_OBJECTS=(.*)$', stdout, re.M)
    expected = ['susfs.o'] + [obj for feature in pins['profiles'][profile] for obj in FEATURE_OBJECTS[feature]]
    if ('KSU_COMPAT_DEVON_SELINUX_POLICY' not in stdout or not selected or
            sorted(selected.group(1).split()) != sorted(expected)):
        raise ValueError('Native Kbuild did not select the exact provider/profile objects')
    if 'KSU_COMPAT_HAS_SUSFS_FEATURE_SELINUX_HIDE' in stdout:
        raise ValueError('Unexpected manual SELinux route')
    return stdout


def validate_image(image):
    with image.open('rb') as stream:
        header = stream.read(64)
    if len(header) != 64 or header[56:60] != b'ARM\x64':
        raise ValueError('Missing ARM64 Image header')
    memory_size = struct.unpack_from('<Q', header, 16)[0]
    if not memory_size or image.stat().st_size <= 64:
        raise ValueError('ARM64 Image size field is invalid')
    # image_size includes zero-initialized memory and may exceed the file size.
    return {'file_bytes': image.stat().st_size, 'memory_bytes': memory_size}


def collect(out, artifacts, pins, env, profile='core'):
    image = out / 'arch/arm64/boot/Image'
    image_info = validate_image(image)
    elf = run(['readelf', '-h', str(out / 'vmlinux')], env=env)
    if 'AArch64' not in elf:
        raise ValueError('vmlinux is not AArch64')
    symbols = (out / 'System.map').read_text()
    if not all(re.search(r'\b(?:__cfi_)?' + prefix + r'\w+(?:[.$]\S+)?$', symbols, re.M) for prefix in ['susfs_', 'ksu_']):
        raise ValueError('Required SUSFS/BakaSU linked symbols are missing')
    modules = sorted(out.rglob('*.ko'))
    trees = sorted((out / 'arch/arm64/boot/dts').rglob('*.dtb')) + sorted((out / 'arch/arm64/boot/dts').rglob('*.dtbo'))
    if not modules or not any('devon' in p.name for p in trees):
        raise ValueError('Module or device-tree outputs are missing')
    verify_config((out / '.config').read_text(), pins, profile)
    shutil.copyfile(image, artifacts / 'Image')
    for name in ['System.map', 'Module.symvers', 'modules.order', 'modules.builtin']:
        shutil.copyfile(out / name, artifacts / name)
    (artifacts / 'vmlinux-elf-header.txt').write_text(elf)
    (artifacts / 'susfs-bakasu-symbols.txt').write_text('\n'.join(line for line in symbols.splitlines() if re.search(r'\b(?:susfs_|ksu_)', line)) + '\n')
    for name, files in [('modules.tar.gz', modules), ('dtbs.tar.gz', trees)]:
        with tarfile.open(artifacts / name, 'w:gz') as bundle:
            for path in files:
                bundle.add(path, arcname=str(path.relative_to(out)), recursive=False)
    return {'status': 'compiled-and-linked', 'image': image_info,
            'modules': len(modules), 'device_trees': len(trees),
            'scope': 'Experimental ' + profile + ' profile compiled and linked; no boot image, boot or runtime validation'}


def save_result(artifacts, record):
    (artifacts / 'result.json').write_text(json.dumps(record, indent=2) + '\n')
    checksums = {p.name: sha(p) for p in sorted(artifacts.iterdir()) if p.is_file() and p.name != 'SHA256SUMS.json'}
    (artifacts / 'SHA256SUMS.json').write_text(json.dumps(checksums, indent=2) + '\n')


def main():
    pins = json.loads(PINS_FILE.read_text())
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument('--prepare', action='store_true', help='Verify dependencies, configure and run Kbuild checks (default)')
    mode.add_argument('--build', action='store_true', help='Also compile Image, dtbs and modules into a fresh output directory')
    parser.add_argument('--jobs', type=int, default=4)
    parser.add_argument('--profile', choices=list(pins['profiles']), default='core',
                        help='Explicit reviewed feature selection; core remains the default')
    parser.add_argument('--clang-dir', type=Path, default=ROOT.parent / '.toolchains/android-16-r547379')
    args = parser.parse_args()
    if args.jobs < 1 or any(c.isspace() for c in str(ROOT)):
        parser.error('Use positive jobs and a Linux source directory without whitespace')
    branch = check_source(pins)
    for command in ['git', 'make', 'gcc', 'g++', 'bc', 'bison', 'flex', 'perl', 'readelf', 'aarch64-linux-gnu-gcc', 'arm-linux-gnueabi-gcc']:
        if not shutil.which(command):
            raise ValueError(f'Missing host dependency: {command}; see docs/SUSFS_2_3_TEST_BUILD.md')
    compiler = prepare_toolchain(args.clang_dir.resolve(), pins)
    repo = prepare_dependency(pins)
    stamp = datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%SZ')
    category = 'susfs-core-test' if args.profile == 'core' else 'susfs-features-test'
    output_parent = ROOT / 'out' / category
    output_parent.mkdir(parents=True, exist_ok=True)
    out = Path(tempfile.mkdtemp(prefix=stamp + '-', dir=output_parent))
    artifacts = ROOT / 'artifacts' / category / out.name
    artifacts.mkdir(parents=True)
    env = os.environ.copy()
    env.update(PATH=str(args.clang_dir.resolve() / 'bin') + os.pathsep + env['PATH'],
               LD_LIBRARY_PATH=str(args.clang_dir.resolve() / 'lib64') + os.pathsep + env.get('LD_LIBRARY_PATH', ''),
               ARCH='arm64', LLVM='1', CLANG_TRIPLE='aarch64-linux-gnu-', CROSS_COMPILE='aarch64-linux-gnu-',
               CROSS_COMPILE_ARM32='arm-linux-gnueabi-', KBUILD_BUILD_USER='devon', KBUILD_BUILD_HOST='susfs-core-test',
               KBUILD_BUILD_VERSION='1', KBUILD_BUILD_TIMESTAMP=run(['git', 'show', '-s', '--format=%cD', 'HEAD']))
    (artifacts / 'compiler.txt').write_text(compiler + '\n')
    versions = {name: run([name, '--version']).splitlines()[0] for name in
                ['make', 'gcc', 'aarch64-linux-gnu-gcc', 'arm-linux-gnueabi-gcc', 'bison', 'flex', 'readelf']}
    (artifacts / 'host-tool-versions.json').write_text(json.dumps(versions, indent=2) + '\n')
    (artifacts / 'build-pins.json').write_bytes(PINS_FILE.read_bytes())
    (artifacts / 'build-driver.py').write_bytes(Path(__file__).read_bytes())
    # Include untracked reviewed inputs too; a Git diff alone can omit them.
    with tarfile.open(artifacts / 'kernel-inputs.tar.gz', 'w:gz') as bundle:
        for name in pins['kernel_files']:
            bundle.add(ROOT / name, arcname=name, recursive=False)
    (artifacts / 'kernel-source.patch').write_bytes(subprocess.check_output(['git', 'diff', pins['kernel_base'], '--binary'], cwd=ROOT))
    (artifacts / 'bakasu-provider.patch').write_bytes((ROOT / pins['provider_patch']).read_bytes())
    print(f'Output: {out}\nEvidence: {artifacts}', flush=True)
    make = ['make', 'O=' + str(out), 'ARCH=arm64', 'LLVM=1']
    run(make + ['vendor/bengal-perf_defconfig'], env=env, log=artifacts / 'defconfig.log')
    run(['bash', 'scripts/kconfig/merge_config.sh', '-m', '-O', str(out), str(out / '.config'),
         *pins['fragments']], env=env, log=artifacts / 'merge-config.log')
    for feature in FEATURE_OBJECTS:
        run(['scripts/config', '--file', str(out / '.config'),
             '--enable' if feature in pins['profiles'][args.profile] else '--disable',
             'KSU_SUSFS_' + feature], env=env)
    run(make + ['olddefconfig'], env=env, log=artifacts / 'olddefconfig.log')
    config = verify_config((out / '.config').read_text(), pins, args.profile)
    shutil.copyfile(out / '.config', artifacts / 'kernel.config')
    kbuild_preflight(out, artifacts, env, pins, args.profile)
    record = {'status': 'prepared-only', 'scope': 'No target object/image built',
              'profile': args.profile, 'features': pins['profiles'][args.profile],
              'kernel_head': run(['git', 'rev-parse', 'HEAD']), 'branch': branch,
              'bakasu_commit': run(['git', 'rev-parse', 'HEAD'], cwd=repo),
              'bakasu_native_revision_count': int(run(['git', 'rev-list', '--count', 'HEAD'], cwd=repo)),
              'out': str(out), 'artifacts': str(artifacts),
              'required_config': {k: config.get(k, 'n') for k in profile_config(pins, args.profile)}}
    (artifacts / 'result.json').write_text(json.dumps(record, indent=2) + '\n')
    if args.build:
        check_source(pins)
        check_dependency(repo, pins)
        record.update(status='building', scope='Target compilation started; no successful image claimed yet')
        (artifacts / 'result.json').write_text(json.dumps(record, indent=2) + '\n')
        print(f'Compiling Image, dtbs and modules; full log: {artifacts / "build.log"}', flush=True)
        stage = 'compilation'
        try:
            run(make + ['-j' + str(args.jobs), 'Image', 'dtbs', 'modules'], env=env, log=artifacts / 'build.log')
            stage = 'artifact-validation'
            check_source(pins)
            check_dependency(repo, pins)
            record.update(collect(out, artifacts, pins, env, args.profile))
        except Exception as error:
            record.update(status='failed', failed_stage=stage, error=str(error))
            save_result(artifacts, record)
            raise
    save_result(artifacts, record)
    print(f'{record["status"]}: {artifacts}', flush=True)
    if not args.build:
        print('To start this ARM64 compile: ./build-susfs-test.sh --build --profile ' + args.profile, flush=True)


if __name__ == '__main__':
    main()
