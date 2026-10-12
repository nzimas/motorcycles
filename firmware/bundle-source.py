#!/usr/bin/env python3
"""Reproducible development source/evidence archive; excludes firmware binaries."""
import pathlib,hashlib,json,zipfile,argparse
ROOT=pathlib.Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser();parser.add_argument('--release',default='R1-rc3');args=parser.parse_args()
OUT=ROOT/'release'/args.release
OUT.mkdir(parents=True,exist_ok=True)
files=[]
for dirname in ['interface','generation','adapter','firmware','tools','validation']:
 for p in (ROOT/dirname).rglob('*'):
  rel=p.relative_to(ROOT)
  if not p.is_file() or any(x in rel.parts for x in ['venv','__pycache__','audio']):continue
  if '.local.' in p.name:continue
  if p.suffix in ['.c','.h','.S','.s','.py','.sh','.ld','.md','.inc'] or p.name.startswith('LICENSE'):files.append(p)
for name in ['README.md','REQUEST.md','architecture.md','Makefile','generation/audio/report.json','generation/audio/rc3-kick/report.json','generation/audio/rc3-kick/compare.py','generation/audio/evaluate_sounds.py','generation/audio/evaluation/rc4-baseline.json','generation/audio/evaluation/rewrite-b70.json','generation/audio/evaluate_kicks.py','generation/audio/evaluation/kicks.json','generation/audio/kick_sweep.py','generation/audio/evaluation/kick_sweep.json','generation/audio/drive_eval.py']:
 p=ROOT/name
 if p.is_file():files.append(p)
for name in ['native-reference-package','native-reference-bootstrap','native-boot-report','native-packaged-live-report','native-packaged-hooks-report','native-popup-report','native-mod-footprints','device-identification','duration-report','fast-blocks-report','native-runtime-report','download-firmware-inventory']:
 p=ROOT/'validation'/(name+'.json')
 if p.is_file():files.append(p)
for rel in ['reference/Modded-Cycles/tools/mtlib/LICENSE','reference/Modded-Cycles/tweaks/model-cycles_OS1.13/LICENSE-Braids','reference/Modded-Cycles/tweaks/model-cycles_OS1.13/LICENSE-Model-TG']:
 files.append(ROOT/rel)
for name in ['selected-configuration-report','selected-boot-report','selected-bootstrap-report','selected-machines-report','native-packaged-live-report','native-packaged-hooks-report','native-popup-report','patch-audition-native-report','pitch-mapping-report','selected-music-lifecycle-report','selected-retune-report','selected-sound-extras-report','selected-lock-persistence-report','selected-music-default-report','selected-drive-report']:
 p=ROOT/'validation'/(name+'.json')
 if p.is_file():files.append(p)
files.append(ROOT/'firmware/selected-configuration.json')
files=sorted(set(files));manifest={'status':args.release+' experimental hardware-test source; no firmware transfer','installed_modification_configuration':json.loads((ROOT/'firmware/selected-configuration.json').read_text()),'vendor_firmware_included':False,'upstream_commit':'d520df4e3ba1dcd6724548c6ff5705b5bd8ac178','model_tg_commit':'70b39dd6787770ebefc7a2d78dea1678ec012679','files':{str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in files}}
entries={str(p.relative_to(ROOT)):p.read_bytes() for p in files};entries['SOURCE-MANIFEST.json']=(json.dumps(manifest,indent=2)+'\n').encode()
archive=OUT/('ModelCycles_'+args.release+'_source.zip')
with zipfile.ZipFile(archive,'w',compression=zipfile.ZIP_DEFLATED,compresslevel=9) as z:
 for name,data in sorted(entries.items()):
  info=zipfile.ZipInfo(name,(2026,10,10,0,0,0));info.compress_type=zipfile.ZIP_DEFLATED;info.external_attr=0o100644<<16;z.writestr(info,data)
with zipfile.ZipFile(archive) as z:
 assert z.testzip() is None
 assert all(not n.endswith(('.syx','.bin','.elf','.wav')) and '.local.' not in n for n in z.namelist())
 for name,data in entries.items():assert z.read(name)==data
sha=hashlib.sha256(archive.read_bytes()).hexdigest();(OUT/'SHA256.txt').write_text(sha+'  '+archive.name+'\n');print(json.dumps({'archive':str(archive),'source_files':len(files),'sha256':sha,'vendor_firmware_included':False},indent=2))
