#!/usr/bin/env python3
"""Actual stock global loader: no persistence claims from opaque padding."""
import pathlib,json,struct
from unicorn import Uc,UC_ARCH_M68K,UC_MODE_BIG_ENDIAN,UC_HOOK_MEM_WRITE
from unicorn import m68k_const as m
from validate_package import read
D=pathlib.Path(__file__).resolve().parent;meta,_,plain=read('/Users/nzimas/Downloads/model-cycles_OS1.13/model-cycles_OS1.13.syx')
u=Uc(UC_ARCH_M68K,UC_MODE_BIG_ENDIAN);u.ctl_set_cpu_model(m.UC_CPU_M68K_CFV4E)
for base,size in [(0x40000000,0x2000000),(0x44000000,0x10000)]:u.mem_map(base,size)
u.mem_write(0x40000400,plain[3]);SRC,DEST,SP,STOP=0x41001000,0x41002000,0x4400ff00,0x44000000
written=set()
def write(u,t,a,n,v,d):
 if DEST<=a<DEST+68:written.update(range(a-DEST,a-DEST+n))
u.hook_add(UC_HOOK_MEM_WRITE,write)
def load(name):
 source=bytearray(range(1,69));source[:16]=name;source[18:20]=(14400).to_bytes(2,'big');source[20:22]=(32).to_bytes(2,'big');source[22:24]=(32).to_bytes(2,'big');source[24]=0;source[25]=0;source[26]=2;source[27]=0
 u.mem_write(SRC,bytes(source));u.mem_write(DEST,b'\xa5'*68);u.mem_write(SP,struct.pack('>3I',STOP,DEST,SRC));u.reg_write(m.UC_M68K_REG_SR,0x2000);u.reg_write(m.UC_M68K_REG_A7,SP);u.emu_start(0x4005b3b4,STOP,count=10000);assert u.reg_read(m.UC_M68K_REG_PC)==STOP;return bytes(u.mem_read(DEST,68))
short=load(b'A\0METADATA123456');full=load(b'ABCDEFGHIJKLMNOP')
assert short[:16]==b'A'+bytes(15),'post-NUL name tail survived unexpectedly';assert full[:16]==b'ABCDEFGHIJKLMNOP'
assert written==set(range(29))|{38}
assert all(short[i]==0xa5 for i in range(68) if i not in written)
# Exact stock save body copies all64globalbytes; no opaque-gap normalization.
FILE,PATTERN=0x41010000,0x41020000
payload=bytes(range(64));u.mem_write(PATTERN+30642,payload);u.mem_write(FILE+14736,b'\xa5'*64)
u.reg_write(m.UC_M68K_REG_A2,FILE);u.reg_write(m.UC_M68K_REG_A3,PATTERN);u.reg_write(m.UC_M68K_REG_A7,SP);u.emu_start(0x4005ba26,0x4005ba3c,count=10000)
assert bytes(u.mem_read(FILE+14736,64))==payload
# Stock default does not clear the proposed owned gap, necessitating a hook.
u.mem_write(DEST,payload+bytes(4));u.mem_write(SP,struct.pack('>3I',STOP,DEST,0));u.reg_write(m.UC_M68K_REG_A7,SP);u.emu_start(0x40061526,STOP,count=10000)
assert bytes(u.mem_read(DEST+40,24))==payload[40:64]
report={'result':'PASS','official_package_sha256':meta['syx_sha256'],'actual_function':'0x4005b3b4','global_write_offsets':sorted(written),'checks':['actual loader retains only global bytes0..28 and38','stock bounded name copy zero-pads after NUL','full sixteen-character name survives','actual save body preserves all64globalbytes','stock default leaves proposed gap40..63 unchanged'],'conclusion':'Offsets29..37 and39..63 serialized on save by64Bcopy but never restored by loader. Post-NUL name metadata is erased. Visible full-name token could persist but consumes user name space and requires rename/copy/clear policy. No unused field proven safe.'}
(D/'pattern-global-storage-report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
