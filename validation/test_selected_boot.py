#!/usr/bin/env python3
"""Execute the exact selected package boot chain (native copier, stock BSS clear with standalone MACRO copier, native startup), offline."""
import pathlib,json,struct
from unicorn import Uc,UC_ARCH_M68K,UC_MODE_BIG_ENDIAN,UC_HOOK_CODE
from unicorn import m68k_const as m
from validate_package import read,sha
D=pathlib.Path(__file__).resolve().parent;C=D.parent/'build/selected-native/ModelCycles-generative-selected'
meta,_,plain=read(C.with_suffix('.syx'));image=plain[3];manifest=json.loads(C.with_suffix('.manifest.json').read_text());assert sha(image)==manifest['main_sha256'];assert meta['syx_sha256']==manifest['package_sha256']
sym={q[2]:int(q[0],16) for line in C.with_suffix('.symbols.txt').read_text().splitlines() if len(q:=line.split())==3}
u=Uc(UC_ARCH_M68K,UC_MODE_BIG_ENDIAN);u.ctl_set_cpu_model(m.UC_CPU_M68K_CFV4E)
for base,size in [(0x40000000,0x2400000),(0x43000000,0x100000),(0x44000000,0x20000),(0xfc040000,0x10000),(0xfc070000,0x1000)]:u.mem_map(base,size)
start,end=0x4019b590,0x423380b0   # stock BSS clear range
u.mem_write(start,b'\xa5'*(end-start));u.mem_write(0x40000400,image);u.mem_write(0x43000000,b'\xa5'*0x100000)
native=image[manifest['source_start']-0x40000400:manifest['source_end']-0x40000400];assert sha(native[:manifest['payload_file_bytes']])==manifest['payload_sha256']
# Standalone MACRO segment table (dest, longs) follows its stub; packed source starts at the stock MAIN end.
parts=[];source=0x401aa140;table=0x4016cb28
while True:
 dest,n=struct.unpack('>2I',u.mem_read(table,8));table+=8
 if not n:break
 parts.append((dest,bytes(u.mem_read(source,n*4))));source+=n*4
SP=0x4401ffe0;u.reg_write(m.UC_M68K_REG_SR,0x2000);u.reg_write(m.UC_M68K_REG_A7,SP);u.mem_write(0xfc044000,struct.pack('>I',0x12345000));u.mem_write(0xfc07000c,struct.pack('>I',123456))
regs=[m.UC_M68K_REG_D4,m.UC_M68K_REG_D5,m.UC_M68K_REG_D6,m.UC_M68K_REG_D7,m.UC_M68K_REG_A2,m.UC_M68K_REG_A3,m.UC_M68K_REG_A4,m.UC_M68K_REG_A5,m.UC_M68K_REG_A6]
for i,r in enumerate(regs):u.reg_write(r,0x11220000+i)
traps=[];seen=set();minimum=[SP]
def hook(u,a,n,d):
 minimum[0]=min(minimum[0],u.reg_read(m.UC_M68K_REG_A7));seen.add(a)
 if bytes(u.mem_read(a,4))==bytes.fromhex('4e7b4005'):
  traps.append({'address':hex(a),'ACR1_value':hex(u.reg_read(m.UC_M68K_REG_D4))});u.reg_write(m.UC_M68K_REG_PC,a+4)
u.hook_add(UC_HOOK_CODE,hook)
# 0x40000530: jsr trampoline (copy native, jsr stock BSS clear -> MACRO copier,
# replay lea, return to 0x4000053a) -> mc_boot_init_hook -> 0x40000540.
u.emu_start(0x40000530,0x40000540,count=60_000_000)
assert u.reg_read(m.UC_M68K_REG_PC)==0x40000540;assert u.reg_read(m.UC_M68K_REG_A7)==SP
assert [u.reg_read(r) for r in regs]==[0x11220000+i for i in range(len(regs))]
assert u.reg_read(m.UC_M68K_REG_D0)==0x12345006 and u.reg_read(m.UC_M68K_REG_D1)==6
assert not any(u.mem_read(start,end-start)),'stock BSS not fully cleared'
assert bytes(u.mem_read(manifest['payload_start'],len(native)))==native
for dest,expected in parts:assert bytes(u.mem_read(dest,len(expected)))==expected
assert not any(u.mem_read(sym['snapshots'],62676));assert all(a in seen for a in manifest['boot_chain']);assert sym['mc_startup'] in seen
report={'result':'PASS','package_sha256':meta['syx_sha256'],'checks':['exact package hashes','boot jsr runs native copier, stock BSS clear with MACRO copier, native startup','native executable copied before its packed source is cleared','all MACRO segments copied before the packed source is cleared','whole stock BSS range cleared','private snapshots initialized zero','callee registers and stock continuation preserved'],'max_observed_stack_bytes':SP-minimum[0],'emulated_control_register_writes':traps,'limitations':'MOVEC ACR1 intercepted; no cache semantics, complete RTOS/peripheral boot or hardware deadlines.'}
(D/'selected-boot-report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
