#!/usr/bin/env python3
"""Actual stock lock getter/setter bounds and canonical slot/file mapping."""
import pathlib,json,struct
from unicorn import Uc,UC_ARCH_M68K,UC_MODE_BIG_ENDIAN
from unicorn import m68k_const as m
from validate_package import read
D=pathlib.Path(__file__).resolve().parent;meta,_,plain=read('/Users/nzimas/Downloads/model-cycles_OS1.13/model-cycles_OS1.13.syx');u=Uc(UC_ARCH_M68K,UC_MODE_BIG_ENDIAN);u.ctl_set_cpu_model(m.UC_CPU_M68K_CFV4E)
for base,size in [(0x40000000,0x2000000),(0x44000000,0x10000)]:u.mem_map(base,size)
u.mem_write(0x40000400,plain[3]);DATA,OWNER,VT,SP,STOP=0x41001000,0x41009000,0x4100a000,0x4400ff00,0x44000000
w=lambda a,v:u.mem_write(a,struct.pack('>I',v&0xffffffff));w(OWNER,VT);w(OWNER+16,DATA);w(VT+40,0x400d2aa4)
def call(addr,*args,end=STOP):
 u.mem_write(SP,struct.pack('>I'+'I'*len(args),STOP,*args));u.reg_write(m.UC_M68K_REG_SR,0x2000);u.reg_write(m.UC_M68K_REG_A7,SP);u.emu_start(addr,end,count=10000);assert u.reg_read(m.UC_M68K_REG_PC)==end;return u.reg_read(m.UC_M68K_REG_D0)
checks=0
for track in range(6):
 for step in (0,1,31,63):
  for slot in (0,1,23,24,32):
   offset=4385*track+68*step+2*slot;u.mem_write(DATA,b'\xff'*26310);u.mem_write(DATA+offset,struct.pack('>H',0x3c80));assert call(0x4000da2a,OWNER,track,step,slot)==0x3c80
   old=bytes(u.mem_read(DATA,26310));call(0x4000dece,OWNER,track,step,slot,0x4580,end=0x4000df40);expected=bytearray(old);expected[offset:offset+2]=struct.pack('>H',0x4580);assert bytes(u.mem_read(DATA,26310))==bytes(expected);checks+=2
for args in ((6,0,24),(0,64,24),(0,0,33)):assert call(0x4000da2a,OWNER,*args)==0xffffffff;checks+=1
mapping=[int.from_bytes(u.mem_read(0x4010ee4c+4*i,4),'big') for i in range(33)];assert mapping[24]==1
report={'result':'PASS','checks':checks,'formula':'locks + track*4385 + step*68 + slot*2; signed16 values','slot24_file_parameter':mapping[24],'limitations':'Setter executes exactly through physical word write before notification. Confirms canonical slot24 maps serialized machine parameter1; identifying that parameter as Pitch still requires verified sound descriptor/engine semantics.'}
(D/'lock-slot-mapping-report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
