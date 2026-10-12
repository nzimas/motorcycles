#!/usr/bin/env python3
"""Exact packaged Patch/Kit commit writes the generated LFO (k1..8) and
Volume+Dist/sends/pan (k19..22) into the live sound, as 8.8 words, and leaves
the header, name, Model-TG words (k23..) and velocity routing untouched."""
import json,os,pathlib,struct
from test_native_live import LiveRig,SOUND,PLAN,REQUEST,candidate_meta
assert os.environ.get('MC_CANDIDATE'),'packaged candidate required'
D=pathlib.Path(__file__).resolve().parent
EXTRA=7756  # mc_plan: sound[6] 60 + phrase[6] 6x1282 + masks 2 + tempo 2
STRIDE=20   # mc_sound_ext: 12 control bytes + 8-byte distortion record
SLOTS=(1,2,3,4,5,6,7,8,19,20,21,22)
checks=[];seen={'lfo':0,'volume_hot':0,'sends':0}
for g in (0,1):
 for path in range(4):
  for seed in range(1,7):
   r=LiveRig();old=r.capture()[1]
   q=bytes((g,path,4,35,80,63,0,0))+struct.pack('>I',seed*7919+path)+bytes((0,0,0,0,0,0,0,32))+bytes(60)+bytes((60,)*6+(63,0));r.u.mem_write(REQUEST,q)
   assert r.call('mc_generate',REQUEST,PLAN)==1
   extra=bytes(r.u.mem_read(PLAN+EXTRA,6*STRIDE));mask=r.u.mem_read(PLAN+7752,1)[0]
   assert r.call('mc_commit_live',0,PLAN,REQUEST)==1,(g,path,seed)
   new=r.capture()[1]
   for t in range(6):
    if not mask&(1<<t):assert new[t]==old[t];continue
    e=extra[STRIDE*t:STRIDE*t+STRIDE]
    for v,k in zip(e,SLOTS):assert new[t][20+2*k:22+2*k]==bytes((v,0)),(g,path,seed,t,k,v)
    assert new[t][76:84]==e[12:20],(g,path,seed,t)  # distortion record
    assert new[t][:20]==old[t][:20] and new[t][66:76]==old[t][66:76] and new[t][84:]==old[t][84:],(g,path,seed,t)
    seen['lfo']+=e[3]!=0;seen['volume_hot']+=e[8]>=100;seen['sends']+=(e[9]|e[10])!=0
   checks.append('generator%d path%d seed%d extras committed'%(g,path,seed))
assert all(seen.values()),seen
report={'result':'PASS','package_sha256':candidate_meta['syx_sha256'],'checks':checks,'observed':seen,
 'limitations':'Exact packaged generator/commit with synthetic owner tree; audible LFO, distortion and send effects are not rendered here.'}
(D/'selected-sound-extras-report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({'result':'PASS','cases':len(checks),'observed':seen},indent=2))
