#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0
"""Collect kernel-only build outputs; never reads or writes a boot image."""
import hashlib,json,os,shutil,subprocess,sys,tarfile
from pathlib import Path
root=Path(__file__).resolve().parents[2]
out,art=map(lambda s:Path(s).resolve(),sys.argv[1:]);art.mkdir(exist_ok=True,parents=True)
image=out/'arch/arm64/boot/Image'
with image.open('rb') as f: header=f.read(64)
assert header[56:60]==b'ARM\x64' and image.stat().st_size>1024*1024,'Invalid ARM64 Image'
shutil.copy2(image,art/'Image')
shutil.copy2(out/'.config',art/'kernel.config')
dtbs=sorted((out/'arch/arm64/boot/dts').rglob('*.dtb'))+sorted((out/'arch/arm64/boot/dts').rglob('*.dtbo'))
modules=sorted(out.rglob('*.ko'))
assert dtbs,'No compiled device trees'
assert modules,'No compiled kernel modules'
for name,files in [('dtbs.tar.gz',dtbs),('modules.tar.gz',modules)]:
    with tarfile.open(art/name,'w:gz') as tar:
        for f in files:tar.add(f,arcname=str(f.relative_to(out)))
(art/'kernel-integration.patch').write_bytes(subprocess.check_output(['git','diff',json.loads((root/'scripts/devon/source-pins.json').read_text())['kernel_commit'],'--binary'],cwd=root))
pins=json.loads((root/'scripts/devon/source-pins.json').read_text())
actual_bakasu=subprocess.check_output(['git','-C',str(root/'KernelSU'),'rev-parse','HEAD']).decode().strip()
pins['bakasu_validated_baseline']=pins['bakasu_commit']
pins['bakasu_commit']=actual_bakasu
pins['bakasu_selection']=os.environ.get('BAKASU_SELECTION','pinned')
(art/'source-pins.json').write_text(json.dumps(pins,indent=2)+'\n')
for relative in ['System.map','Module.symvers','modules.order','modules.builtin',
                 'include/config/kernel.release','include/generated/compile.h',
                 'include/generated/utsrelease.h','include/generated/uapi/linux/version.h']:
    destination=art/'build-metadata'/relative
    destination.parent.mkdir(parents=True,exist_ok=True)
    shutil.copy2(out/relative,destination)
symbols=subprocess.check_output(['llvm-nm','--defined-only',str(out/'vmlinux')]).decode()
ksu=[line for line in symbols.splitlines() if ' ksu_' in line]
assert ksu,'No linked BakaSU symbols in vmlinux'
(art/'bakasu-symbols.txt').write_text('\n'.join(ksu)+'\n')
manifest={'kernel_base':json.loads((root/'scripts/devon/source-pins.json').read_text())['kernel_commit'],
          'kernel_checkout':subprocess.check_output(['git','rev-parse','HEAD'],cwd=root).decode().strip(),
          'bakasu':actual_bakasu,
          'image_bytes':image.stat().st_size,'device_trees':len(dtbs),'kernel_modules':len(modules),'linked_bakasu_symbols':len(ksu),
          'validation':'Kernel Image/dtbs/modules compile and link; optional boot packaging recorded separately; no device test',
          'artifacts':{}}
for f in sorted(art.iterdir()):
    if f.is_file() and f.name not in ['build-result.json','SHA256SUMS.txt']:
        manifest['artifacts'][f.name]={'bytes':f.stat().st_size,'sha256':hashlib.file_digest(f.open('rb'),'sha256').hexdigest()}
for f in sorted((art/'build-metadata').rglob('*')):
    if f.is_file():
        with f.open('rb') as stream:
            manifest['artifacts'][str(f.relative_to(art))]={'bytes':f.stat().st_size,'sha256':hashlib.file_digest(stream,'sha256').hexdigest()}
(art/'build-result.json').write_text(json.dumps(manifest,indent=2)+'\n')
(art/'SHA256SUMS.txt').write_text(''.join(v['sha256']+'  '+k+'\n' for k,v in manifest['artifacts'].items()))
print(json.dumps({k:v for k,v in manifest.items() if k!='artifacts'},indent=2))
