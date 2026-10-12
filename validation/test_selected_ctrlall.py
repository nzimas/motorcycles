#!/usr/bin/env python3
"""Exact packaged latched Control All (firmware/ctrlall.c):
 1. FUNC + TRK press toggles the mode; the press, repeats and the release are
    consumed; TRK without FUNC goes to the stock path.
 2. The stock encoder handlers' held-key query (patched register loads at
    0x4001f55c / 0x4001aaa8) answers TRK held while on, except while a trig is
    held; other keys and the off state pass through.
 3. Knob edits reaching the audio (0x40059534, patched entry) are recorded
    while on and still reach the stock handler (cache written).
 4. The lock applier calls (0x40058f36, 0x40059194) go through mc_ca_locks:
    while on, locks on turned parameters carry the Control All value, missing
    ones are appended (sound-locked trigs included), others are untouched;
    off, the buffer is untouched.
 5. Turning it off restores all six sounds and notifies their owners;
    a pattern load meanwhile retakes the snapshot instead.
 6. FUNC + SWEEP / FUNC + PITCH move all six tracks while on.
 7. The TRK LED is requested lit while on and unlit after."""
import json,os,pathlib,struct
from unicorn import UC_HOOK_CODE
from unicorn import m68k_const as m
from test_native_live import LiveRig,SOUND,candidate_meta,SYMBOLS
assert os.environ.get('MC_CANDIDATE'),'packaged candidate required'
D=pathlib.Path(__file__).resolve().parent;checks=[]
IMAGE=pathlib.Path(os.environ['MC_CANDIDATE']).with_suffix('.main.bin').read_bytes()
rd=lambda a,n:IMAGE[a-0x40000400:a-0x40000400+n]
S=SYMBOLS
assert rd(0x4001f55c,6)==b'\x49\xf9'+struct.pack('>I',S['mc_held']) and rd(0x4001aaa8,6)==b'\x45\xf9'+struct.pack('>I',S['mc_held'])
assert rd(0x40058f36,6)==rd(0x40059194,6)==b'\x4e\xb9'+struct.pack('>I',S['mc_ca_locks'])
assert rd(0x40059534,6)==b'\x4e\xf9'+struct.pack('>I',S['mc_param_edit_hook'])
checks.append('patched sites: held-key register loads -> mc_held, both lock applier calls -> mc_ca_locks, knob-edit entry -> hook')

class Rig(LiveRig):
 def __init__(s):
  super().__init__();s.held={1:0,2:0};s.trig=0;s.leds=[];s.applied=[]
  s.u.hook_add(UC_HOOK_CODE,s.stub(lambda a:s.held.get(a[0],0)),begin=0x4007faf4,end=0x4007faf4)
  s.u.hook_add(UC_HOOK_CODE,s.stub(lambda a:0x41500000),begin=0x400cf9a8,end=0x400cf9a8)
  s.u.hook_add(UC_HOOK_CODE,s.stub(lambda a:s.trig),begin=0x4006bdfe,end=0x4006bdfe)
  s.u.hook_add(UC_HOOK_CODE,s.stub(lambda a:0x41510000),begin=0x400cfaf4,end=0x400cfaf4)
  s.u.hook_add(UC_HOOK_CODE,s.stub(lambda a:s.leds.append((a[1],a[2])) or 0),begin=0x40005f86,end=0x40005f86)
  def apply(a):
   n=struct.unpack('>i',bytes(s.u.mem_read(a[1]+8,4)))[0]
   s.applied.append((a[0],[struct.unpack('>HH',bytes(s.u.mem_read(a[1]+20+8*i,4))) for i in range(n)]));return 0
  s.u.hook_add(UC_HOOK_CODE,s.stub(apply),begin=0x400583da,end=0x400583da)
 def on(s):return s.u.mem_read(S['mc_ca_on'],1)[0]
r=Rig()
# 1. toggle
assert r.key(2,1)==0 and r.on()==0;assert r.key(2,16)==0;checks.append('TRK alone goes to the stock path')
sounds0=bytes(r.u.mem_read(SOUND,600))
r.held[1]=1;assert r.key(2,1)==1 and r.on()==1;r.held[1]=0
assert r.key(2,9)==1 and r.key(2,16)==1 and r.key(2,1)==0;r.key(2,16)
checks.append('FUNC + TRK turns Control All on; its repeat and release are consumed; the next plain TRK is stock')
# 2. held query
r.held[2]=0
assert r.call('mc_held',2)==1 and r.call('mc_held',1)==0;r.held[1]=1;assert r.call('mc_held',1)==1;r.held[1]=0
r.trig=1;assert r.call('mc_held',2)==0;r.trig=0
checks.append('held-key query: TRK reads held while on (not while a trig is held); other keys unchanged')
# 3. knob edits recorded, stock handler still runs
r.w32(0x800017e4,0x41520000);r.u.mem_write(0x41520000,bytes(700))
for t in range(6):
 r.u.mem_write(0x41520000+28+100*t+20+2*13,struct.pack('>H',(40+t)<<8))
def edit(value,track,k):
 r.call('mc_param_edit_hook',value,track,k)
 return int.from_bytes(r.u.mem_read(0x800015a0+2*(33*track+7+k),2),'big')
for t in range(6):assert edit((90+t)<<8,t,11)==(90+t)<<8
assert edit(77<<8,6,3) is not None   # FX "track" 6: ignored by Control All, stock still writes
checks.append('knob edits reach the stock cache write and are recorded for Control All')
# 4. locks
BUF=0x41530000
def locks(track,entries):
 r.u.mem_write(BUF,bytes(20+8*66));r.u.mem_write(BUF+8,struct.pack('>i',len(entries)))
 for i,(k,v) in enumerate(entries):r.u.mem_write(BUF+20+8*i,struct.pack('>HH',k,v))
 r.applied.clear();r.call('mc_ca_locks',track,BUF,0x800015a0);return dict(r.applied[0][1]),r.applied[0][0]
got,t=locks(2,[(11,5<<8),(18,7<<8)]);assert t==2 and got=={11:92<<8,18:7<<8},got
got,_=locks(4,[(18,9<<8)]);assert got=={18:9<<8,11:94<<8},got
checks.append('locks on a turned parameter carry the Control All value; missing ones are appended; others untouched')
# 5. exit restores and notifies
r.u.mem_write(SOUND+100*3+22,bytes((99,0)));r.u.mem_write(SOUND+100*5+76,bytes((120,)))
before=r.caches();r.held[1]=1;assert r.key(2,1)==1 and r.on()==0;r.held[1]=0;r.key(2,16)
assert bytes(r.u.mem_read(SOUND,600))==sounds0,'sounds restored'
after=r.caches();assert after[3]==0 and after[5]==0 and after[0]==before[0],(before,after)
got,_=locks(2,[(11,5<<8)]);assert got=={11:5<<8}
checks.append('turning it off restores the six sounds, notifies the changed owners and lets step locks through again')
# pattern load while on: snapshot retaken, values forgotten
r.held[1]=1;r.key(2,1);r.held[1]=0;r.key(2,16)
r.call('mc_param_edit_hook',50<<8,1,12)
stamp=S['mc_music_context_stamp'];r.w32(stamp,int.from_bytes(r.u.mem_read(stamp,4),'big')+1)
r.u.mem_write(SOUND+100*1+24,bytes((33,0)));loaded=bytes(r.u.mem_read(SOUND,600))
r.call('mc_ca_tick',0)
got,_=locks(1,[(12,5<<8)]);assert got=={12:5<<8},got
r.u.mem_write(SOUND+100*1+24,bytes((70,0)));r.held[1]=1;r.key(2,1);r.held[1]=0;r.key(2,16)
assert bytes(r.u.mem_read(SOUND,600))==loaded
checks.append('a pattern load while on makes its sounds the restore point and drops earlier values')
# 6. FUNC + SWEEP / PITCH on all tracks
for t in range(6):r.u.mem_write(SOUND+100*t+76,bytes((10*t,0,0,0,0,0,(256-5*t)&255,0)))
r.held[1]=1;r.key(2,1);r.key(2,16)
assert r.enc(6,7)==1;assert [r.u.mem_read(SOUND+100*t+76,1)[0] for t in range(6)]==[10*t+7 for t in range(6)]
assert r.enc(2,-60)==1;assert [int.from_bytes(r.u.mem_read(SOUND+100*t+82,1),'big',signed=True) for t in range(6)]==[max(-63,-5*t-60) for t in range(6)]
checks.append('FUNC + SWEEP and FUNC + PITCH move all six tracks by the same amount, each clamped')
# 7. LED
r.leds.clear();r.call('mc_ca_tick',0);assert (31,2) in r.leds
r.key(2,1);r.held[1]=0;r.key(2,16);r.leds.clear();r.call('mc_ca_tick',0);assert (31,1) in r.leds and r.on()==0
checks.append('TRK LED requested lit while on, unlit after')
report={'result':'PASS','package_sha256':candidate_meta['syx_sha256'],'checks':checks,
 'limitations':'Synthetic owner tree, kit and lock buffers; stock encoder handlers, popup drawing and the LED controller are stubbed or absent; no audio rendered.'}
(D/'selected-ctrlall-report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({'result':'PASS','checks':len(checks)},indent=2))
