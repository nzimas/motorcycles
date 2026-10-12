#!/usr/bin/env python3
"""Exact packaged entry hooks with real native handlers and synthetic OS owners.
Stops stock fallbacks at the replay continuation; does not emulate full UI OS.
"""
import os,pathlib,json,struct
D=pathlib.Path(__file__).resolve().parent
os.environ.setdefault('MC_CANDIDATE',str(D.parent/'build/native-stock-reference/native-stock-reference'))
from test_native_live import LiveRig,candidate_meta
from test_native_runtime import ROOT,EVENT,SP,STOP
from unicorn import m68k_const as m
checks=[]
def hook(r,entry,args,consumed,continuation=None,frame=0):
 r.w32(0xfc07000c,r.raw);r.u.mem_write(SP,struct.pack('>I'+'I'*len(args),STOP,*args))
 r.u.reg_write(m.UC_M68K_REG_SR,0x2000);r.u.reg_write(m.UC_M68K_REG_A7,SP)
 r.u.emu_start(entry,STOP if consumed else continuation,count=1_000_000)
 assert r.u.reg_read(m.UC_M68K_REG_PC)==(STOP if consumed else continuation)
 assert r.u.reg_read(m.UC_M68K_REG_A7)==(SP+4 if consumed else SP-frame)
 if consumed:assert r.u.reg_read(m.UC_M68K_REG_D0)==1

def key(r,code,flags,consumed):
 r.u.mem_write(EVENT,bytes(32));r.w32(EVENT+12,code);r.w32(EVENT+16,flags)
 hook(r,0x400060fe,[ROOT,EVENT],consumed,0x40006106,16)
def pad(r,index,down,consumed):
 r.u.mem_write(EVENT,bytes(32));r.w32(EVENT+20,index);r.w32(EVENT+16,down)
 hook(r,0x4000614c,[ROOT,EVENT],consumed,0x40006152)
 if not consumed:assert int.from_bytes(r.u.mem_read(SP+4,4),'big')==ROOT+64
for g in range(4):
 r=LiveRig();before=r.capture();key(r,15,1,False);pad(r,g+1,1,True);pad(r,g+1,0,True)
 key(r,18,1,True);key(r,18,16,True);assert r.ui(62+g)==3  # step 3 sets level 3
 key(r,31,1,True)                                          # step 16 is the generate key
 r.advance(1499);key(r,31,8,True);assert r.capture()==before
 r.advance(1);key(r,31,8,True);after=r.capture();assert after!=before and r.ui(11)==1
 r.advance(1000);key(r,31,8,True);assert r.capture()==after;key(r,31,16,True)
 key(r,15,16,True);assert r.ui(0)!=0;key(r,15,1,True);key(r,15,16,True);assert r.ui(0)==0
 pad(r,6,1,False);key(r,16,1,False)
 checks.append('generator%d real patched key/pad entries, level key, step16 1499/1500ms, single publication, cancellation and ordinary fallbacks'%g)
r=LiveRig();key(r,15,1,False);pad(r,3,1,True);pad(r,3,0,True);key(r,16,1,True);key(r,16,16,True);assert r.ui(62+2)==1
r.u.mem_write(EVENT,bytes(32));r.w32(EVENT+12,2);r.delta=7
hook(r,0x40006158,[ROOT,EVENT],False,0x40006160,72);assert r.ui(62+2)==1 and r.ui(0)!=0
r.u.mem_write(EVENT,bytes(32));r.w32(EVENT+12,3);hook(r,0x40006158,[ROOT,EVENT],False,0x40006160,72)
r.u.mem_write(EVENT,bytes(32));r.w32(EVENT+12,4);hook(r,0x40006158,[ROOT,EVENT],False,0x40006160,72)
checks.append('exact encoder entry replays Pitch/Decay (former Density/Brutality) and other encoder fallbacks in generator mode')
# Light setter rewrites arg3 before replaying the original three-register frame.
r.u.mem_write(SP,struct.pack('>4I',STOP,ROOT,600,7));r.u.reg_write(m.UC_M68K_REG_A7,SP)
r.u.emu_start(0x40005f86,0x40005f8e,count=10000)
assert r.u.reg_read(m.UC_M68K_REG_PC)==0x40005f8e and r.u.reg_read(m.UC_M68K_REG_A7)==SP-12
assert int.from_bytes(r.u.mem_read(SP+12,4),'big') in (1,2)
checks.append('exact light setter rewrites owned LED mode before original prologue')
prior=r.capture();r.call('mc_led_hook');assert r.capture()==prior
checks.append('exact vtable LED callback paints without publishing')
report={'result':'PASS','package_sha256':candidate_meta['syx_sha256'],'checks':checks,'limitations':'Exact packaged entry bytes/native handlers; synthetic OS accessors and owners. Ordinary stock handlers stop at verified replay continuation. No full OS boot or peripheral timing.'}
(D/'native-packaged-hooks-report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
