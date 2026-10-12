#!/usr/bin/env python3
"""Execute the bootstrap decoder from the exact supplied package, without rebuilding it.

This proves loader decompression, not full instrument boot or peripherals.
"""
import argparse,hashlib,json,pathlib,struct
from unicorn import Uc,UC_ARCH_M68K,UC_MODE_BIG_ENDIAN,UC_HOOK_MEM_UNMAPPED,UC_HOOK_MEM_WRITE
from unicorn import m68k_const as m
from validate_package import read

def main():
 p=argparse.ArgumentParser();p.add_argument('package');p.add_argument('--out',required=True);a=p.parse_args()
 meta,stored,plain=read(a.package);want=plain[3];compressed=stored[3];boot=plain[2]
 STAGE,DEST,STOP,SP=0x40200000,0x40000400,0x8001f000,0x8001e000
 assert DEST+len(want)<=STAGE,'decompressed MAIN OS reaches bootstrap transit region'
 uc=Uc(UC_ARCH_M68K,UC_MODE_BIG_ENDIAN);uc.ctl_set_cpu_model(m.UC_CPU_M68K_ANY)
 uc.mem_map(0x80000000,0x20000);uc.mem_write(0x800003fc,boot);uc.mem_map(0x40000000,0x400000)
 uc.mem_write(STAGE,compressed);uc.mem_write(SP,struct.pack('>III',STOP,STAGE,DEST));uc.reg_write(m.UC_M68K_REG_A7,SP)
 guard=b'\xa5'*64;uc.mem_write(DEST-64,guard);uc.mem_write(DEST+len(want),guard)
 bad=[];bad_writes=[];write_count=0
 def write(u,access,addr,size,value,data):
  nonlocal write_count
  write_count+=1
  if not (DEST<=addr and addr+size<=DEST+len(want) or 0x80010000<=addr and addr+size<=SP+12):bad_writes.append((addr,size))
 uc.hook_add(UC_HOOK_MEM_WRITE,write)
 uc.hook_add(UC_HOOK_MEM_UNMAPPED,lambda u,t,addr,size,value,d:bad.append((addr,size)) or False)
 uc.emu_start(0x800006bc,STOP,count=400_000_000)
 assert uc.reg_read(m.UC_M68K_REG_PC)==STOP,'decoder exhausted instruction budget'
 assert bytes(uc.mem_read(DEST,len(want)))==want,'bootstrap result differs from independent aPLib decoder'
 assert uc.reg_read(m.UC_M68K_REG_D0)==len(want),'returned decompressed length differs'
 assert bytes(uc.mem_read(DEST-64,64))==guard and bytes(uc.mem_read(DEST+len(want),64))==guard,'output guards corrupted'
 assert not bad and not bad_writes, f'invalid accesses {bad[:5]} {bad_writes[:5]}'
 assert uc.reg_read(m.UC_M68K_REG_A7)==SP+4,'unbalanced decoder stack'
 report={'result':'PASS','package':str(pathlib.Path(a.package).resolve()),'package_sha256':meta['syx_sha256'],'decompressed_main_sha256':hashlib.sha256(want).hexdigest(),'compressed_bytes':len(compressed),'decompressed_bytes':len(want),'main_end':hex(DEST+len(want)),'transit_start':hex(STAGE),'transit_margin_bytes':STAGE-DEST-len(want),'memory_writes':write_count,'invalid_accesses':bad,'invalid_writes':bad_writes,'limitations':'Actual protected section-2 decoder executed on exact packaged section-3 bytes. Does not exercise reset, RTOS, UI startup or peripherals.'}
 pathlib.Path(a.out).write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
if __name__=='__main__':main()
