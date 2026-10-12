#!/usr/bin/env python3
"""Exercise verified stock global semantic getter/setter family with canary gap."""
import struct,json,pathlib
from unicorn import Uc,UC_ARCH_M68K,UC_MODE_BIG_ENDIAN,UC_HOOK_MEM_READ,UC_HOOK_MEM_WRITE
from unicorn import m68k_const as m
from validate_package import read
D=pathlib.Path(__file__).resolve().parent;meta,_,plain=read('/Users/nzimas/Downloads/model-cycles_OS1.13/model-cycles_OS1.13.syx');u=Uc(UC_ARCH_M68K,UC_MODE_BIG_ENDIAN);u.ctl_set_cpu_model(m.UC_CPU_M68K_CFV4E)
for base,size in [(0x40000000,0x2000000),(0x44000000,0x10000)]:u.mem_map(base,size)
u.mem_write(0x40000400,plain[3]);DATA,OWNER,VT,RET,SP,STOP=0x41001000,0x41002000,0x41003000,0x41004000,0x4400ff00,0x44000000
w=lambda a,v:u.mem_write(a,struct.pack('>I',v&0xffffffff))
w(OWNER,VT);w(OWNER+16,DATA);w(VT+40,0x400d2aa4);w(VT+16,RET);u.mem_write(RET,bytes.fromhex('4e75'));reads=set();writes=set()
def access(target):
 def hook(u,t,a,n,v,d):
  if DATA<=a<DATA+64:target.update(range(a-DATA,a-DATA+n))
 return hook
u.hook_add(UC_HOOK_MEM_READ,access(reads));u.hook_add(UC_HOOK_MEM_WRITE,access(writes))
def call(addr,*args):
 u.mem_write(SP,struct.pack('>I'+'I'*len(args),STOP,*[a&0xffffffff for a in args]));u.reg_write(m.UC_M68K_REG_SR,0x2000);u.reg_write(m.UC_M68K_REG_A7,SP);u.emu_start(addr,STOP,count=100000);assert u.reg_read(m.UC_M68K_REG_PC)==STOP
getters=[0x4000c816,0x4000c8a6,0x4000c8dc,0x4000c964,0x4000c994,0x4000ca64,0x4000caf2,0x4000cb22,0x4000cc2a,0x4000ccc0,0x4000ccfa,0x4000cf20]
setters=[0x4000c846,0x4000c90c,0x4000c9c4,0x4000ca94,0x4000cb40,0x4000cba6,0x4000cc5a,0x4000cd2a,0x4000cdae,0x4000cf50]
checks=0
for scale in (0,1):
 for val in (-1,0,1,6,64,1024,40000):
  data=bytearray(b'\xa5'*64);data[:16]=b'UNCHANGED-NAME123';data[18:20]=(14400).to_bytes(2,'big');data[20:24]=b'\x00\x20\x00\x20';data[25]=scale;data[26]=2;u.mem_write(DATA,bytes(data))
  for addr in getters:call(addr,OWNER,1)
  for addr in setters:call(addr,OWNER,val,1)
  assert bytes(u.mem_read(DATA+40,24))==b'\xa5'*24;checks+=len(getters)+len(setters)
assert not any(i>=40 for i in reads|writes)
report={'result':'PASS','checks':checks,'actual_getters':[hex(a) for a in getters],'actual_setters':[hex(a) for a in setters],'read_offsets':sorted(reads),'write_offsets':sorted(writes),'stock_sha256':meta['syx_sha256'],'limitations':'Verified semantic global family under boundary values and scale modes; does not mathematically prove every indirect stock/TG access across all OS paths.'}
(D/'global-semantics-report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
