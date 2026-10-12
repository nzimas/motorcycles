"""Render the Rhythmic Kit's track-1 kick from two generator builds through the
stock OS1.13 voice loop and compare punch measures. Offline; no device access.

  validation/venv/bin/python generation/audio/evaluate_kicks.py OLD.dylib NEW.dylib [--seeds 16] [--wav DIR]

The voice loop excludes Volume+Dist, the PUNCH amp stage and sends, so these
numbers cover the machine controls only; on the unit RC7 kicks also get the
hotter drive floor and PUNCH.
"""
from pathlib import Path
import argparse,ctypes,json,sys
import numpy as np
sys.path.insert(0,str(Path(__file__).resolve().parent))
import evaluate_sounds as ev
def kit_kick(lib,seed,brutality):
 q=ev.Request();q.generator=1;q.path=1;q.target=0;q.density=40;q.brutality=brutality;q.machine_mask=63;q.seed=seed
 buf=(ctypes.c_uint8*8192)()
 assert lib.mc_generate(ctypes.byref(q),buf)==1
 return dict(zip(ev.FIELDS,bytes(buf[:10])))
def pattern_note(lib,seed,sound):
 # The note a Rhythmic Pattern from the same build plays on track 1 with this kick.
 q=ev.Request();q.generator=3;q.path=1;q.target=0;q.density=40;q.brutality=55;q.machine_mask=63;q.seed=seed^0x5bd1e995
 q.machine_valid_mask=1;q.current_machine[0]=sound['machine'];q.source_sound_valid_mask=1
 for k in ev.FIELDS:setattr(q.current_sound[0],k,sound[k])
 buf=(ctypes.c_uint8*8192)();assert lib.mc_generate(ctypes.byref(q),buf)==1
 step=20;base=60+2  # mc_plan: sound[6] then phrase[0]: length, role, 64 x 20-byte steps (old builds: 4-byte steps)
 for size in (20,4):
  for i in range(64):
   on,note=buf[base+size*i],buf[base+size*i+1]
   if on==1 and 30<=note<=127:return note,size
 return 60,0
def punch(x):
 y=x.astype(float)/2**31;sr=48000
 env=np.sqrt(np.convolve(y*y,np.ones(48)/48,'same'))
 attack=float(env[:int(0.015*sr)].max());body=float(np.sqrt(np.mean(y[int(0.05*sr):int(0.25*sr)]**2)))+1e-12
 seg=y[:int(0.15*sr)];spec=np.abs(np.fft.rfft(seg*np.hanning(len(seg))))**2;f=np.fft.rfftfreq(len(seg),1/sr)
 def centroid(a,b):
  s=y[int(a*sr):int(b*sr)];s=s-s.mean();S=np.abs(np.fft.rfft(s*np.hanning(len(s))));F=np.fft.rfftfreq(len(s),1/sr);return float(np.sum(F*S)/(np.sum(S)+1e-12))
 return {'attack_peak':round(attack,4),'transient_db':round(20*np.log10(attack/body),2),'low_share_150hz':round(float(spec[f<150].sum()/spec.sum()),3),
  'sweep_ratio':round(centroid(0,0.01)/max(centroid(0.06,0.12),1),2),'peak':round(float(np.abs(y).max()),4)}
def main():
 a=argparse.ArgumentParser();a.add_argument('old');a.add_argument('new');a.add_argument('--seeds',type=int,default=16);a.add_argument('--brutality',type=int,default=55);a.add_argument('--wav')
 a=a.parse_args();libs={'old':ctypes.CDLL(a.old),'new':ctypes.CDLL(a.new)}
 for l in libs.values():l.mc_generate.argtypes=[ctypes.POINTER(ev.Request),ctypes.c_void_p]
 rows=[];wav=Path(a.wav) if a.wav else None
 if wav:wav.mkdir(parents=True,exist_ok=True)
 for k in range(a.seeds):
  seed=(0x6379636c+0x9e3779b9*(k+1))&0xffffffff
  for name,lib in libs.items():
   s=kit_kick(lib,seed,a.brutality)
   if s['machine']>5:rows.append({'build':name,'seed':seed,'params':s,'rendered':False});continue
   note,_=pattern_note(lib,seed,s)
   e=ev.E.Engine(ev.STOCK);e.solo(0);e.set(0,**s,note=note);x=e.render(int(400*1.5)+64,trig_at=(1,))[64:]
   m=punch(x);m['pattern_note']=note;m['sounding_note']=note+s['pitch']-64;rows.append({'build':name,'seed':seed,'params':s,'rendered':True,'metrics':m})
   if wav and k<8:ev.write_wav(wav/f'{name}-kick-{k+1:02d}.wav',x)
 summary={}
 for name in libs:
  r=[q['metrics'] for q in rows if q['build']==name and q['rendered']]
  summary[name]={key:float(np.median([m[key] for m in r])) for key in r[0]}|{'rendered':len(r),'sounding_notes':sorted(m['sounding_note'] for m in r)}
 out={'brutality':a.brutality,'summary':summary,'rows':rows};Path(__file__).resolve().parent.joinpath('evaluation','kicks.json').write_text(json.dumps(out,indent=1)+'\n')
 print(json.dumps(summary,indent=1))
if __name__=='__main__':main()
