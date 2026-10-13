#!/usr/bin/env python3
"""Exact popup assembly ABI and integration, mocked UI constructors/heap.
No claim of real framebuffer rendering or completed RTOS boot.
"""
import pathlib,os,json,struct
D=pathlib.Path(__file__).resolve().parent
os.environ.setdefault('MC_CANDIDATE',str(D.parent/'build/native-stock-reference/native-stock-reference'))
from test_native_live import LiveRig,candidate_meta
from test_native_runtime import ROOT,VIEW
from unicorn import UC_HOOK_CODE
class PopupRig(LiveRig):
 def __init__(self):
  super().__init__();self.texts=[];self.releases=0;self.allocated={}
  self.w32(0x40fe4178,1)
  for addr,fn in {0x400f980c:self.string,0x400747c0:self.construct,0x4007700e:self.present,0x400cf23c:self.release,0x400f7d5c:lambda a:0}.items():self.u.hook_add(UC_HOOK_CODE,self.stub(fn),begin=addr,end=addr)
 def alloc(self,args):
  size=args[0]
  if size>156:return LiveRig.alloc(self,args)   # commit snapshots (firmware/live.c), not popup memory
  assert size in (8,16,156)
  address=0x410c0000+256*self.heap;self.heap+=1
  self.u.mem_write(address,bytes(size))
  if hasattr(self,'allocated'):self.allocated[address]=size
  return address
 def string(self,args):self.w32(args[0],args[1]);return args[0]
 def construct(self,args):assert self.allocated[args[0]]==156;self.w32(args[0]+152,self.value_at(args[1]));return args[0]
 def value_at(self,a):return int.from_bytes(self.u.mem_read(a,4),'big')
 def present(self,args):
  assert args[0]==VIEW and args[2]==0
  popup=self.value_at(args[1]);holder=self.value_at(args[1]+4)
  assert self.allocated[holder]==16
  assert [self.value_at(holder+4*i) for i in range(4)]==[0x400fbf40,1,1,popup]
  text=bytes(self.u.mem_read(self.value_at(popup+152),32)).split(b'\0')[0].decode();self.texts.append(text);return 0
 def release(self,args):assert self.value_at(args[0]) in self.allocated;self.releases+=1;return 0
checks=[]
for g,name in enumerate(['PATCH','KIT','PHRASE','PATTERN']):
 r=PopupRig();r.key(15,1);r.pad(g+1,1);r.pad(g+1,0);r.key(31,1);r.advance(1499);r.key(31,8);assert not r.texts and r.ui(11)==0
 r.advance(1);r.key(31,8)
 assert r.texts==[name+' CREATED'] and r.releases==1 and r.ui(11)==1
 r.advance(1000);r.key(31,8);assert len(r.texts)==1
 checks.append(name+' step16 1500ms hold: successful publication shows one standard-lifetime popup with valid String/popup/holder ABI')
# Enter generator mode while stopped (PAGE during playback is the live break), then start playback: generation is refused.
r=PopupRig();r.key(15,1);r.pad(4,1);r.pad(4,0);r.running=1;r.key(31,1);r.advance(1500);r.key(31,8)
assert not r.texts and r.ui(11)==2
checks.append('failed publication never shows a success popup')
r=PopupRig();r.w32(0x40fe4178,0);r.call('mc_show_popup',0x41000000);assert not r.texts and r.heap==0
checks.append('popup machinery unavailable returns without allocation')
report={'result':'PASS','package_sha256':candidate_meta['syx_sha256'],'checks':checks,'limitations':'Exact packaged assembly and native commit integration, mocked heap/String/Popup/present/reference release. Framebuffer and complete view-stack runtime not emulated.'}
(D/'native-popup-report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
