#!/usr/bin/env python3
"""Exact packaged scale/root retune (mc_retune_live) on a synthetic pattern.
Checks note mapping against an independent reference, pitch-lock offsets,
byte scope, the scale tag and refusals (no Model-TG on this build)."""
import json,os,pathlib,random
from test_native_live import LiveRig,PATTERN,SOUND,candidate_meta
assert os.environ.get('MC_CANDIDATE'),'packaged candidate required'
D=pathlib.Path(__file__).resolve().parent
INTERVALS=[(0,2,4,5,7,9,11),(0,2,3,5,7,8,10),(0,2,4,6,8,10),(0,2,3,7,8)]
def in_scale(n,root,scale):return (n+12-root)%12 in INTERVALS[scale]
def quantize(note,root,scale,lo,hi):
 cands=[n for n in range(lo,hi+1) if in_scale(n,root,scale)]
 return min(cands,key=lambda n:(abs(n-note),n))
def retune(note,old,new,scale,lo,hi):
 d=new-old;d=d-12 if d>6 else d+12 if d<-5 else d
 return quantize(min(127,max(0,note+d)),new,scale,lo,hi)
def effective(raw,offset,old,new,scale):
 e=min(127,max(0,raw+offset));lo=offset if offset>0 else 0;hi=127+offset if offset<0 else 127
 return retune(e,old,new,scale,lo,hi)-offset
def unpack(music):return (9,1) if music==0 else (music&15,(music>>4)&3)
checks=[];rng=random.Random(4)
def setup(slice_track):
 r=LiveRig();raw=bytearray(r.u.mem_read(PATTERN,30710))
 for i in range(6):
  t=722*i;raw[t+712]=rng.randrange(40,90)
  for s in range(64):
   raw[t+2*s:t+2*s+2]=b'\x00\x00';raw[t+644+s]=255;raw[t+580+s]=255
   raw[4332+4385*i+68*s+20:4332+4385*i+68*s+22]=b'\xff\xff'
  for s in rng.sample(range(64),4):  # lock-only trigs (0x6801): retune must not touch them
   raw[t+2*s:t+2*s+2]=b'\x68\x01';raw[t+580+s]=rng.choice([255,70])
  for s in rng.sample(range(64),12):
   raw[t+2*s:t+2*s+2]=b'\x00\x00';raw[t+2*s+1]=1;raw[t+580+s]=rng.choice([255,rng.randrange(30,100)])
   if rng.random()<0.3:raw[4332+4385*i+68*s+20]=rng.randrange(52,77)  # pitch lock offset
  r.u.mem_write(SOUND+100*i+40,bytes((rng.randrange(56,73),0)))
 r.u.mem_write(PATTERN,bytes(raw));return r
for case in range(24):
 slice_track=6  # no Model-TG slice tracks on this build: every track is retuned
 r=setup(slice_track);old=bytes(r.u.mem_read(PATTERN,30710))
 sounds=[bytes(r.u.mem_read(SOUND+100*i+40,2)) for i in range(6)]
 new_music=128|(rng.randrange(4)<<4)|rng.randrange(12);old_root,_=unpack(0);new_root,new_scale=unpack(new_music)
 assert r.call('mc_retune_live',0,0,new_music)==1,case
 new=bytes(r.u.mem_read(PATTERN,30710));expect=bytearray(old)
 for i in range(6):
  if i==slice_track:continue
  t=722*i;offset=sounds[i][0]-64;default=effective(old[t+712],offset,old_root,new_root,new_scale);expect[t+712]=default
  for s in range(64):
   if not old[t+2*s+1]&1 or old[t+2*s]==0x68:continue
   stored=old[t+580+s];rawn=old[t+712] if stored==255 else stored
   lock=old[4332+4385*i+68*s+20:4332+4385*i+68*s+22];so=offset if lock==b'\xff\xff' else lock[0]-64
   note=effective(rawn,so,old_root,new_root,new_scale)
   if stored!=255 or note!=default:expect[t+580+s]=note
 tag=new[30682:30690];expect[30682:30690]=tag
 assert tag[:5]==b'MCG4\x01' and tag[5]==new_music and tag[6]==new_music^0xa5 and tag[7]==(new_music+0x37)&255,tag.hex()
 assert new==bytes(expect),(case,next(k for k in range(30710) if new[k]!=expect[k]))
 if slice_track<6:assert new[722*slice_track:722*(slice_track+1)]==old[722*slice_track:722*(slice_track+1)]
 assert len(r.notifications)==(6 if slice_track==6 else 5)+1,(case,len(r.notifications))  # tracks + global
 assert r.call('mc_music_read',PATTERN+30642)&255==new_music
 # Second retune from the stored setting to another scale/root.
 second=128|(rng.randrange(4)<<4)|rng.randrange(12)
 assert r.call('mc_retune_live',0,new_music,second)==1 and r.call('mc_music_read',PATTERN+30642)&255==second
checks.append('24 packaged retunes match independent reference notes (default, explicit, inherited, slot-10 pitch-locked steps; lock-only trigs untouched)')
checks.append('only note bytes and the 8-byte scale tag change; chained second retune from stored setting')
for failure in ('running','stale_old','invalid_new','pitch_out_of_range'):
 r=setup(6);music=0x80;old_music=0
 if failure=='running':r.running=1
 if failure=='stale_old':old_music=0x85
 if failure=='invalid_new':music=0x8c
 if failure=='pitch_out_of_range':r.u.mem_write(SOUND+40,bytes((30,0)))
 old=bytes(r.u.mem_read(PATTERN,30710))
 assert r.call('mc_retune_live',0,old_music,music)==0,failure
 assert bytes(r.u.mem_read(PATTERN,30710))==old and not r.notifications,failure
 checks.append(failure+' refused without change')
report={'result':'PASS','package_sha256':candidate_meta['syx_sha256'],'checks':checks,'limitations':'Exact packaged MAIN/payload with synthetic owner tree and transport stub; Model-TG loop_mode written directly. No audio render, UI repaint or hardware timing.'}
(D/'selected-retune-report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
