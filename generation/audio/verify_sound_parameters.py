"""Verify final core still generates the exact sound parameters rendered in report."""
from pathlib import Path
import ctypes,hashlib,json,struct
ROOT=Path(__file__).resolve().parent
report=json.loads((ROOT/'report.json').read_text())
lib=ctypes.CDLL(str(ROOT.parent/'generation-host.dylib'))
lib.mc_generate.argtypes=[ctypes.c_void_p,ctypes.c_void_p]
keys=('machine','pitch','color','shape','sweep','contour','punch','gate','finetune','decay')
for item in report['examples']:
 path=('abstract','rhythmic','melodic','free').index(item['path'])
 raw=struct.pack('@6B2xI8B',0,path,0,30,35,63,item['seed'],0,*([0]*6),0)+bytes(68)
 assert len(raw)==88
 request=ctypes.create_string_buffer(raw);plan=ctypes.create_string_buffer(1612)
 assert lib.mc_generate(request,plan)==1
 assert dict(zip(keys,plan.raw[:10]))==item['params'],item['file']
report['final_core_rendered_sound_parameter_parity']=True
report['final_core_parameter_check_source_sha256']=hashlib.sha256((ROOT.parent/'generation.c').read_bytes()).hexdigest()
(ROOT/'report.json').write_text(json.dumps(report,indent=2)+'\n')
print('PASS: final core exactly matches all 12 rendered sound parameter sets')
