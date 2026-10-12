#!/usr/bin/env python3
"""Execute real compiled ColdFire bulk helpers, guards and callee-saved ABI."""
import pathlib,json,hashlib
from test_native_runtime import Rig,SYM,D
from unicorn import m68k_const as m
r=Rig();A,B=0x41070000,0x41080000;checks=0
regs=[m.UC_M68K_REG_D2,m.UC_M68K_REG_D3,m.UC_M68K_REG_D4,m.UC_M68K_REG_D5,m.UC_M68K_REG_D6,m.UC_M68K_REG_D7,m.UC_M68K_REG_A2,m.UC_M68K_REG_A3,m.UC_M68K_REG_A4,m.UC_M68K_REG_A5,m.UC_M68K_REG_A6]
def call(name,*args):
 global checks
 expected={reg:0x12340000+i for i,reg in enumerate(regs)}
 for reg,val in expected.items():r.u.reg_write(reg,val)
 result=r.call(name,*args)
 assert all(r.u.reg_read(reg)==val for reg,val in expected.items()),name+' damagedcallee-savedregister';checks+=1;return result
for n in (32,64,96,512,30720):
 data=bytes((i*17+3)&255 for i in range(n));r.u.mem_write(A-16,b'\xa5'*16+data+b'\x5a'*16);r.u.mem_write(B-16,b'\xc3'*16+b'\x00'*n+b'\x3c'*16)
 call('mc_fw_copy_blocks',B,A,n)
 assert bytes(r.u.mem_read(B,n))==data
 assert bytes(r.u.mem_read(A-16,n+32))==b'\xa5'*16+data+b'\x5a'*16
 assert bytes(r.u.mem_read(B-16,16))==b'\xc3'*16 and bytes(r.u.mem_read(B+n,16))==b'\x3c'*16
 assert call('mc_fw_equal_blocks',A,B,n)==1
 for offset in ([0,n-1,n//2] if n!=512 else range(n)):
  r.u.mem_write(B+offset,bytes((data[offset]^1,)));assert call('mc_fw_equal_blocks',A,B,n)==0,(n,offset);r.u.mem_write(B+offset,data[offset:offset+1])
report={'result':'PASS','callee_checked_calls':checks,'all512byte_mismatch_offsets_checked':True,'copy_sizes':[32,64,96,512,30720],'max_observed_stack':0x4400ffe0-r.min_sp,'elf_sha256':hashlib.sha256((D/'native-runtime.elf').read_bytes()).hexdigest(),'limitations':'Actual compiled bulkhelper assembly; directABI tests. Not publicationobserver, ISR deadline or finalpackage proof.'}
(D/'fast-blocks-report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
