"""Map the stock KICK machine's punch against its controls, through the stock
OS1.13 voice loop. One control varies at a time around a neutral kick.
Offline; no device access.

  validation/venv/bin/python generation/audio/kick_sweep.py
"""
from pathlib import Path
import json,sys
import numpy as np
sys.path.insert(0,str(Path(__file__).resolve().parent))
import evaluate_sounds as ev
BASE=dict(machine=0,pitch=64,color=10,shape=16,sweep=16,contour=24,punch=1,gate=0,finetune=64,decay=60)
def measure(x):
 y=x.astype(float)/2**31;sr=48000
 def band(a,b,lo,hi):
  s=y[int(a*sr):int(b*sr)];S=np.abs(np.fft.rfft(s*np.hanning(len(s))))**2;f=np.fft.rfftfreq(len(s),1/sr)
  return float(S[(f>=lo)&(f<hi)].sum()),float(S.sum()),f[np.argmax(S*(f>20))]
 low,total,f0=band(0.06,0.30,30,150)
 env=np.sqrt(np.convolve(y*y,np.ones(240)/240,'same'));peak=env.max()+1e-12
 body_ms=float(np.count_nonzero(env>peak*0.1))/48
 attack=float(env[:int(0.015*sr)].max());body=float(np.sqrt(np.mean(y[int(0.05*sr):int(0.25*sr)]**2)))+1e-12
 return {'f0_hz':round(float(f0),1),'low_share':round(low/(total+1e-24),3),'low_energy':low,'body_ms':round(body_ms,1),
  'transient_db':round(20*np.log10(attack/body),2),'peak':round(float(np.abs(y).max()),4)}
rows=[]
def run(**change):
 p=dict(BASE,**change);x=ev.render(p,ms=320);m=measure(x);rows.append({'params':p,'metrics':m});print(json.dumps({**change,**m}),flush=True)
for v in (48,52,56,60,64,68,72,76):run(pitch=v)
for v in (0,32,64,96,127):run(sweep=v)
for v in (0,32,64,96,127):run(contour=v)
for v in (0,32,64,96,127):run(shape=v)
for v in (0,32,64,96,127):run(color=v)
for v in (20,40,80,100,127):run(decay=v)
Path(__file__).resolve().parent.joinpath('evaluation','kick_sweep.json').write_text(json.dumps(rows,indent=1)+'\n')
