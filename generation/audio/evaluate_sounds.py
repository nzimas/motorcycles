"""Render many generated Patch sounds through the stock OS1.13 voice loop and
score character and variety. Offline; no device access.

  validation/venv/bin/python generation/audio/evaluate_sounds.py [--seeds 24] [--wav DIR] [--label NAME]

Uses generation/generation-host.dylib (rebuild with `make -C generation generation-host.dylib`).
MACRO (machine 7) and Sampler (6) are not rendered: the stock voice loop has no MACRO.
"""
from pathlib import Path
import argparse,ctypes,hashlib,json,os,sys,wave
import numpy as np
ROOT=Path(__file__).resolve().parent;PROJECT=ROOT.parent.parent
TOOLS=PROJECT/'reference/Modded-Cycles/tools'
sys.path[:0]=[str(TOOLS),str(TOOLS/'emu')]
os.environ['PATH']=str(ROOT/'bin')+os.pathsep+os.environ['PATH']
import mcengine as E
from test_sdvintage import main_os_from_syx
FIRMWARE=Path(os.environ.get('MC_STOCK_SYX',str(PROJECT/'model-cycles_OS1.13.syx')))
assert hashlib.sha256(FIRMWARE.read_bytes()).hexdigest()=='44fe586269631a0ca7da25a3383fc6733c314809505fc3cc52f1e0ed9800640c'
STOCK=main_os_from_syx(FIRMWARE)
FIELDS=('machine','pitch','color','shape','sweep','contour','punch','gate','finetune','decay')
class Sound(ctypes.Structure):_fields_=[(n,ctypes.c_uint8) for n in FIELDS]
class Request(ctypes.Structure):
 _fields_=[(n,ctypes.c_uint8) for n in ('generator','path','target','density','brutality','machine_mask')]+[('source_sound_valid_mask',ctypes.c_uint8),('music',ctypes.c_uint8),('seed',ctypes.c_uint32),('machine_valid_mask',ctypes.c_uint8),('current_machine',ctypes.c_uint8*6),('phrase_length',ctypes.c_uint8),('current_sound',Sound*6),('default_note',ctypes.c_uint8*6),('note_valid_mask',ctypes.c_uint8),('pad',ctypes.c_uint8)]
LIB=ctypes.CDLL(str(PROJECT/'generation/generation-host.dylib'))
PATHS=('abstract','rhythmic','melodic','free')
def generate(path,seed,brutality,generator=0,target=0,mask=63):
 q=Request();q.generator=generator;q.path=path;q.target=target;q.density=40;q.brutality=brutality;q.machine_mask=mask;q.seed=seed;q.music=0
 buf=(ctypes.c_uint8*8192)();
 if LIB.mc_generate(ctypes.byref(q),buf)!=1:return None
 return [dict(zip(FIELDS,bytes(buf[10*i:10*i+10]))) for i in range(6)]
def render(params,ms=400):
 e=E.Engine(STOCK);e.solo(0);e.set(0,**params,note=60)
 x=e.render(int(ms*1.5)+64,trig_at=(1,))[64:]
 assert not e.unmapped;return x
def measure(x):
 y=x.astype(float)/2**31;peak=float(np.max(np.abs(y)))+1e-12;rms=float(np.sqrt(np.mean(y*y)))
 seg=y[:min(len(y),9600)];seg=seg-seg.mean();spec=np.abs(np.fft.rfft(seg*np.hanning(len(seg))));f=np.fft.rfftfreq(len(seg),1/48000);p=spec**2+1e-24
 env=np.sqrt(np.convolve(y*y,np.ones(480)/480,'same'));audible=env>peak*0.05
 return {'peak':round(peak,4),'rms':round(rms,5),'crest_db':round(20*np.log10(peak/(rms+1e-12)),2),
  'centroid_hz':round(float(np.sum(f*spec)/(np.sum(spec)+1e-12)),1),
  'flatness':round(float(np.exp(np.mean(np.log(p[1:])))/np.mean(p[1:])),4),
  'hf_ratio':round(float(p[f>4000].sum()/p.sum()),4),'sub_ratio':round(float(p[f<120].sum()/p.sum()),4),
  'length_ms':round(float(np.count_nonzero(audible))/48,1),'silent':peak<1e-6}
def write_wav(path,x):
 y=np.clip(x.astype(float)/2**31*32767*4,-32768,32767).astype('<i2')  # fixed +12dB, no per-file normalization
 with wave.open(str(path),'wb') as w:w.setnchannels(1);w.setsampwidth(2);w.setframerate(48000);w.writeframes(y.tobytes())
def main():
 a=argparse.ArgumentParser();a.add_argument('--seeds',type=int,default=24);a.add_argument('--brutality',type=int,default=70)
 a.add_argument('--wav');a.add_argument('--label',default='current');a.add_argument('--out',default=str(ROOT/'evaluation'))
 a=a.parse_args();out=Path(a.out);out.mkdir(exist_ok=True);wavdir=Path(a.wav) if a.wav else None
 if wavdir:wavdir.mkdir(parents=True,exist_ok=True)
 rows=[]
 for path in range(4):
  for k in range(a.seeds):
   seed=0x6379636c+0x9e3779b9*(k+1)&0xffffffff   # the device's first generations after boot
   sounds=generate(path,seed,a.brutality)
   if not sounds:continue
   s=sounds[0]
   if s['machine']>5:rows.append({'path':PATHS[path],'seed':seed,'params':s,'rendered':False});continue
   x=render(s);m=measure(x);rows.append({'path':PATHS[path],'seed':seed,'params':s,'rendered':True,'metrics':m})
   if wavdir and k<6:write_wav(wavdir/f'{a.label}-{PATHS[path]}-{k+1:02d}-m{s["machine"]}.wav',x)
 summary={}
 for name in PATHS:
  r=[q for q in rows if q['path']==name and q.get('rendered')]
  if not r:continue
  M=lambda key:[q['metrics'][key] for q in r]
  P=np.array([[q['params'][k] for k in ('color','shape','sweep','contour','decay','pitch')] for q in r],float)
  summary[name]={'rendered':len(r),'machines':{str(m):sum(q['params']['machine']==m for q in rows if q['path']==name) for m in range(8)},
   'median_centroid_hz':float(np.median(M('centroid_hz'))),'centroid_spread_oct':round(float(np.std(np.log2(np.array(M('centroid_hz'))+1))),3),
   'median_flatness':float(np.median(M('flatness'))),'median_hf_ratio':float(np.median(M('hf_ratio'))),'median_crest_db':float(np.median(M('crest_db'))),
   'param_mean_abs_from_64':round(float(np.mean(np.abs(P[:,:5]-64))),2),'param_extreme_share':round(float(np.mean((P[:,:5]<12)|(P[:,:5]>115))),3),
   'param_pairwise_distance':round(float(np.mean([np.abs(P[i,:5]-P[j,:5]).mean() for i in range(len(P)) for j in range(i)])),2)}
 report={'label':a.label,'brutality':a.brutality,'summary':summary,'rows':rows}
 (out/f'{a.label}.json').write_text(json.dumps(report,indent=1)+'\n');print(json.dumps(summary,indent=1))
if __name__=='__main__':main()
