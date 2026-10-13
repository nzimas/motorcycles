#!/usr/bin/env python3
"""Actual compiled live preparation/publication and stock sound observer/defaults.
Uses synthetic initialized owner trees and selected-pattern/transport/heap bindings.
"""
import json,pathlib,struct,hashlib,os
from unicorn import UC_HOOK_CODE
from test_native_runtime import Rig,D,ROOT,VTABLE,SOUND
from validate_package import read
STOCK=os.environ.get('MC_STOCK_SYX','/Users/nzimas/Downloads/model-cycles_OS1.13/model-cycles_OS1.13.syx')
try:meta,stored,plain=read(STOCK)
except PermissionError:
 # Candidate runs execute the packaged MAIN, never stock bytes; only the
 # report's stock hash is needed, taken from the recorded official baseline.
 assert os.environ.get('MC_CANDIDATE'),'stock firmware unreadable and no candidate image'
 meta=json.loads((D/'baseline.json').read_text())['stock'];stored=plain=None
SYMBOLS={l.split()[2]:int(l.split()[0],16) for l in (D/'native-live-symbols.txt').read_text().splitlines()}
CANDIDATE=os.environ.get('MC_CANDIDATE')
if CANDIDATE:
 C=pathlib.Path(CANDIDATE);SYMBOLS={l.split()[2]:int(l.split()[0],16) for l in C.with_suffix('.symbols.txt').read_text().splitlines()};candidate_meta,_,candidate_plain=read(str(C.with_suffix('.syx')));IMAGE=candidate_plain[3]
else:IMAGE=None
OWNER,PATTERN,PLAN,REQUEST=0x41120000,0x41130002,0x41190000,0x41198000
class LiveRig(Rig):
 def __init__(self,raw=0):
  self.masked_count=0;self.counting_mask=False
  super().__init__(raw=raw,symbols=SYMBOLS,binary=D/'native-live.bin',main_image=IMAGE);
  if IMAGE is None:self.u.mem_write(0x40000400,plain[3])
  self.u.mem_map(0x80000000,0x10000)
  self.running=0;self.stopcalls=0;self.injection=None;self.notifications=[];self.heap=0;self.parentcalls=0
  self.w32(VTABLE+16,0x40015020);self.w32(VTABLE+36,0x400d2a8e);self.w32(VTABLE+40,0x400d2aa4)
  for owner in [OWNER,OWNER+44,OWNER+640]+[OWNER+112+88*i for i in range(6)]:self.w32(owner,VTABLE)
  self.w32(OWNER+16,PATTERN);self.w32(OWNER+44+16,PATTERN+30642);self.w32(OWNER+640+16,PATTERN+4332)
  raw=bytearray(b'\xa5'*30710);raw[30660:30662]=(120*120).to_bytes(2,'big');raw[30662:30664]=(32).to_bytes(2,'big');raw[30667]=0;raw[30668]=2
  for i in range(6):
   t=OWNER+112+88*i;self.w32(t+16,PATTERN+722*i);self.w32(t+44,OWNER+640);self.w32(t+56,i);self.w32(t+60,OWNER)
   raw[722*i+712]=64;raw[722*i+713:722*i+715]=(32).to_bytes(2,'big');raw[722*i+715]=2
   o=ROOT+212+68*i;self.w32(o,0x400fef54);self.w32(o+16,SOUND+100*i);self.u.mem_write(o+12,b'\x00\x00');self.u.mem_write(o+36,b'\x00')
   vector=0x41090000+32*i;functor=0x410a0000+8*i;self.w32(o+20,vector);self.w32(o+24,vector+16);self.w32(o+28,vector+16);self.u.mem_write(vector,struct.pack('>4I',functor,0,0x4000e572,0x4000e2ec));self.u.mem_write(functor,struct.pack('>2I',ROOT,i))
   sound=bytearray(b'\x55'*100);sound[38]=i;sound[39]=0;self.u.mem_write(SOUND+100*i,bytes(sound));self.w32(0x800015a0+(147+i)*4,0x12340000+i)
  self.u.mem_write(PATTERN,bytes(raw))
  for machine in range(6):self.u.mem_write(0x40a71540+(machine+1)*76,struct.pack('>19I',0x40fe4278,0x40fe4278,*([42,50,46,47,48,49,44,45,43]+[0]*8)))
  for addr,fn in {0x4000f208:lambda a:OWNER,0x4005481a:self.transport,0x40015020:self.notified,0x400d0f6c:self.parent,0x400802e0:self.alloc,0x400802ec:lambda a:0}.items():self.u.hook_add(UC_HOOK_CODE,self.stub(fn),begin=addr,end=addr)
 def stack(self,u,a,n,d):
  super().stack(u,a,n,d)
  if a==self.symbols['mc_irq_save']:self.counting_mask=True
  if self.counting_mask:self.masked_count+=1
  if a==self.symbols['mc_irq_restore']:self.counting_mask=False;self.masked_count+=2
 def transport(self,args):
  self.stopcalls+=1
  if self.injection and self.stopcalls==2:self.injection(self)
  return self.running
 def notified(self,args):self.notifications.append(args[0]);return 0
 def parent(self,args):self.parentcalls+=1;return 0
 def alloc(self,args):
  # 8-byte popup objects, or the ~62 KB commit snapshots (firmware/live.c).
  if args[0]!=8:
   assert args[0]<0x20000
   try:self.u.mem_map(0x41a00000,0x20000)
   except Exception:pass
   self.big=getattr(self,'big',0)+1;return 0x41a00000
  v=0x410b0000+16*self.heap;self.heap+=1;return v
 def capture(self):return bytes(self.u.mem_read(PATTERN,30710)),[bytes(self.u.mem_read(SOUND+100*i,100)) for i in range(6)]
 def generate(self,g,path,target=4,music=0):
  request=bytes((g,path,target,35,50,63,0,music))+(123+g*4+path).to_bytes(4,'big')+bytes((63,0,1,2,3,4,5,32))+bytes(60)+bytes((60,60,60,60,60,60,63,0));self.u.mem_write(REQUEST,request);assert self.call('mc_generate',REQUEST,PLAN)==1
 def caches(self):return [int.from_bytes(self.u.mem_read(0x800015a0+(147+i)*4,4),'big') for i in range(6)]
checks=[];max_stack=0;masked_max=[0]*4
for g in range(4):
 for path,music in [(p,m) for p in range(4) for m in (0,0x95)]:
  r=LiveRig();r.generate(g,path,music=music);old,old_sounds=r.capture();assert r.call('mc_commit_live',0,PLAN,REQUEST)==1,(g,path);new,new_sounds=r.capture()
  for i in range(6):
   if g==2 or g==0 and i!=4:assert new_sounds[i]==old_sounds[i]
   if g==3:assert new_sounds[i][:58]==old_sounds[i][:58] and new_sounds[i][58:60]==b'\x7f\x00' and new_sounds[i][60:]==old_sounds[i][60:]  # Pattern: VOLUME+DIST 127 only
   if g<2 or g==2 and i!=4:
    assert new[722*i:722*(i+1)]==old[722*i:722*(i+1)]
    assert new[4332+4385*i:4332+4385*(i+1)]==old[4332+4385*i:4332+4385*(i+1)]
  # RC4: every generation also publishes the pattern's own 8-byte scale tag
  # (global+40..47); all other global bytes are unchanged outside Pattern.
  # A chosen scale is published as the pattern's 8-byte tag (global+40..47);
  # with no choice (music 0) the tag stays exactly as it was (RC7).
  tag=new[30682:30690]
  if music:assert tag[:5]==b'MCG4\x01' and tag[5]==music and tag[6]==tag[5]^0xa5 and tag[7]==(tag[5]+0x37)&255,(g,path,tag.hex())
  else:assert tag==old[30682:30690],(g,path,tag.hex())
  if g!=3:assert new[30642:30682]==old[30642:30682] and new[30690:]==old[30690:]
  assert len(r.notifications)==(0 if g<2 else 2 if g==2 else 8)+(1 if music and g<3 else 0)  # sound owners notify via the cache, not this spy
  assert r.caches()==[0 if g in (1,3) or g==0 and i==4 else 0x12340000+i for i in range(6)],(g,path,music,r.caches(),len(r.notifications))
  assert r.parentcalls==(1 if g==0 else 6 if g in (1,3) else 0)
  masked_max[g]=max(masked_max[g],r.masked_count)
  max_stack=max(max_stack,0x4400ffe0-r.min_sp);checks.append('generator%d/path%d canonicalscope and real soundcacheobserver'%(g,path))
for g in range(4):
 # PAGE + pad enters on the captured track; step 3 sets level 3 (mc_interface.level[] at +62);
 # step 16 (key 31) held 1500 ms generates once.
 r=LiveRig();old=r.capture();r.key(15,1);r.pad(g+1,1);r.pad(g+1,0);r.selected=0;assert r.key(18,1)==1 and r.key(18,16)==1 and r.ui(62+g)==3 and r.capture()==old
 r.key(31,1);r.advance(1499);r.key(31,8);assert r.capture()==old;r.advance(1);r.key(31,8);assert r.ui(11)==1 and r.capture()!=old;committed=r.capture();r.advance(1000);r.key(31,8);assert r.capture()==committed;r.key(31,16);r.key(15,16);assert r.ui(0)!=0;r.key(15,1);r.key(15,16);assert r.ui(0)==0
 max_stack=max(max_stack,0x4400ffe0-r.min_sp);checks.append('generator%d actual nativeinput to concrete livepublication'%g)
for failure in ('running','invalidscale','malformedevent','stalebyte','childrebind','lockrebind','indexrebind','parentrebind'):
 r=LiveRig();r.generate(2,1);old=r.capture()
 if failure=='running':r.running=1
 if failure=='invalidscale':r.u.mem_write(PATTERN+30667,b'\x02');old=r.capture()
 if failure=='malformedevent':r.u.mem_write(PLAN+60+4*1282+2+2,b'\x00') # mc_phrase is 1282 bytes (20-byte steps); first step may inactive
 if failure=='malformedevent':r.u.mem_write(PLAN+60+4*1282+2,bytes((1,60,0,1)))
 if failure=='stalebyte':r.injection=lambda rig:rig.u.mem_write(PATTERN+722*4+10,b'\x00')
 if failure=='childrebind':r.injection=lambda rig:rig.w32(OWNER+112+88*4+16,PATTERN+722*3)
 if failure=='metadatarebind':r.injection=lambda rig:rig.w32(OWNER+112+88*4+56,3)
 if failure=='lockrebind':r.injection=lambda rig:rig.w32(OWNER+112+88*4+44,OWNER+640+4)
 if failure=='indexrebind':r.injection=lambda rig:rig.w32(OWNER+112+88*4+56,3)
 if failure=='parentrebind':r.injection=lambda rig:rig.w32(OWNER+112+88*4+60,OWNER+4)
 ret=r.call('mc_commit_live',0,PLAN,REQUEST)
 assert ret==0,failure+' should reject before publication'
 new=r.capture()
 if failure=='stalebyte':expected=bytearray(old[0]);expected[722*4+10]=0;assert new==(bytes(expected),old[1])
 else:assert new==old,failure+' changed canonicalcontent'
 assert not r.notifications and r.caches()==[0x12340000+i for i in range(6)]
 checks.append(failure+' refusal preserved canonicaldata/observers')
report={'result':'PASS','checks':checks,'max_observed_stack_bytes':max_stack,'stock_sha256':meta['syx_sha256'],'elf_sha256':hashlib.sha256((C.with_suffix('.elf') if CANDIDATE else D/'native-live.elf').read_bytes()).hexdigest(),'limitations':'Actual live.c/preparation/transaction, stock machine defaults and real sound manager/cache observer; initialized synthetic owner tree, track/global/lock spies, selectedpattern/transport and heap stubs. No finalpackage or completeaudioISR.'}
report['max_masked_instruction_count_by_generator']=dict(zip(['Patch','Kit','Phrase','Pattern'],masked_max))
report['instruction_timing_limit']='Instruction counts cover IRQ-save entry through restore (conservatively including the save prologue) and stock transport/binding revalidation. They are not hardware cycle measurements or an ISR deadline proof.'
report['exact_package_sha256']=candidate_meta['syx_sha256'] if CANDIDATE else None
report['limitations']='Exact package MAIN and linked payload; stock defaults and sound observer with synthetic owner trees, transport, heap and track/global notification spies. Not complete boot or audio ISR.' if CANDIDATE else report['limitations']
(D/('native-packaged-live-report.json' if CANDIDATE else 'native-live-report.json')).write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
