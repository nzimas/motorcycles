#!/usr/bin/env python3
"""Exact reference MAIN clear, native startup and stock cache initialization.
No peripherals, full RTOS boot or hardware timing proof.
"""
import pathlib,json,struct,hashlib,os
from unicorn import Uc,UC_ARCH_M68K,UC_MODE_BIG_ENDIAN
from unicorn import m68k_const as m
from validate_package import read
D=pathlib.Path(__file__).resolve().parent
C=pathlib.Path(os.environ.get('MC_CANDIDATE',str(D.parent/'build/native-stock-reference/native-stock-reference')))
meta,_,plain=read(C.with_suffix('.syx'));image=plain[3]
manifest=json.loads(C.with_suffix('.manifest.json').read_text())
assert hashlib.sha256(image).hexdigest()==manifest['reference_main_sha256']
sym={q[2]:int(q[0],16) for line in C.with_suffix('.symbols.txt').read_text().splitlines() if len(q:=line.split())==3}
u=Uc(UC_ARCH_M68K,UC_MODE_BIG_ENDIAN);u.ctl_set_cpu_model(m.UC_CPU_M68K_CFV4E)
u.mem_map(0x40000000,0x2400000);u.mem_map(0x44000000,0x10000);u.mem_map(0x45000000,0x1000);u.mem_map(0xfc070000,0x1000)
u.mem_write(0x40000400,image);u.reg_write(m.UC_M68K_REG_SR,0x2000)
SP=0x4400ffe0;STOP=0x45000000
start,end=manifest['boot_clear_start'],manifest['boot_clear_end'];skip0,skip1=manifest['boot_clear_skip']
u.mem_write(start,b'\xa5'*(end-start))
u.mem_write(0x40000400,image)
kept=bytes(u.mem_read(skip0,skip1-skip0))
u.mem_write(SP,struct.pack('>5I',0x1234,0x2345,0x3456,0x4567,STOP))
u.reg_write(m.UC_M68K_REG_A7,SP);u.reg_write(m.UC_M68K_REG_A0,start)
for r in [m.UC_M68K_REG_D4,m.UC_M68K_REG_D5,m.UC_M68K_REG_D6,m.UC_M68K_REG_D7]:u.reg_write(r,0)
u.emu_start(0x400004d2,STOP,count=30_000_000)
assert u.reg_read(m.UC_M68K_REG_PC)==STOP
assert bytes(u.mem_read(skip0,skip1-skip0))==kept
assert not any(u.mem_read(start,skip0-start)) and not any(u.mem_read(skip1,end-skip1))
assert u.reg_read(m.UC_M68K_REG_A7)==SP+20
assert [u.reg_read(r) for r in [m.UC_M68K_REG_D4,m.UC_M68K_REG_D5,m.UC_M68K_REG_D6,m.UC_M68K_REG_D7]]==[0x1234,0x2345,0x3456,0x4567]
def call(address):
 u.mem_write(SP,struct.pack('>I',STOP));u.reg_write(m.UC_M68K_REG_A7,SP);u.emu_start(address,STOP,count=2_000_000)
 assert u.reg_read(m.UC_M68K_REG_PC)==STOP and u.reg_read(m.UC_M68K_REG_A7)==SP+4
bss=manifest['bss_start'];u.mem_write(bss,b'\xa5'*(skip1-bss));u.mem_write(0xfc07000c,struct.pack('>I',123456));call(sym['mc_startup'])
assert bytes(u.mem_read(skip0,bss-skip0))==kept[:bss-skip0]
# The large private staging area must be zeroed, while initialized controller
# fields intentionally contain seed/configuration values.
assert not any(u.mem_read(sym['snapshots'],62676))
# Invoke the patched boot hook itself and verify its stock replay continuation.
u.mem_write(SP,struct.pack('>I',STOP));u.reg_write(m.UC_M68K_REG_A7,SP)
u.reg_write(m.UC_M68K_REG_A0,0x44001000);u.mem_write(0x44001000,struct.pack('>I',0x12345000))
u.reg_write(m.UC_M68K_REG_A1,0x12345678)
u.emu_start(0x4000053a,0x40000540,count=2_000_000)
assert u.reg_read(m.UC_M68K_REG_PC)==0x40000540
assert u.reg_read(m.UC_M68K_REG_D0)==0x12345006 and u.reg_read(m.UC_M68K_REG_D1)==6
assert u.reg_read(m.UC_M68K_REG_A0)==0x44001000 and u.reg_read(m.UC_M68K_REG_A1)==0x12345678
assert u.reg_read(m.UC_M68K_REG_A7)==SP
call(0x4007929c)
base=manifest['cache_metadata_base'];retired=manifest['cache_blocks_retired']
word=lambda a:int.from_bytes(u.mem_read(a,4),'big')
head=word(0x401eb894);tail=word(0x401eb890);assert head==base+20*retired
nodes=[];node=head
while node:
 assert node not in nodes and base<=node<base+320 and (node-base)%20==0
 nodes.append(node);node=word(node+12)
assert nodes==[base+20*i for i in range(retired,16)] and nodes[-1]==tail
assert word(head+16)==0
assert bytes(u.mem_read(skip0,bss-skip0))==kept[:bss-skip0]
report={'result':'PASS','package_sha256':meta['syx_sha256'],'checks':['exact patched clear entry preserves payload reservation and clears original remaining region','clear loop restores stock callee saved registers and stack','native startup initializes persistent BSS without altering text','patched boot hook initializes and replays stock continuation with preserved stack','stock cache initializer links only unreserved blocks'],'cache_nodes':len(nodes),'limitations':'Exact MAIN code; no full RTOS/peripheral boot or ISR timing.'}
(D/'native-boot-report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
