#!/usr/bin/env python3
"""Execute official OS primitives. Synthetic owner getters/notifications; no full boot proof."""
import pathlib,struct,sys
from unicorn import Uc,UC_ARCH_M68K,UC_MODE_BIG_ENDIAN,UC_HOOK_CODE
from unicorn import m68k_const as R
sys.path.insert(0,str(pathlib.Path(__file__).resolve().parent))
from verify_mapping import verify,BASE
ROOT=pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'reference/Modded-Cycles/tools'))
from mtlib import aplib,container
from mtlib.syx import unwrap
raw=pathlib.Path(sys.argv[1]).read_bytes();verify(sys.argv[1]);s,_=unwrap(raw);c=container.parse(s);sec=next(s for s in c['sections'] if s['id']==3);main=aplib.depack(c['blob'][sec['off']:sec['off']+sec['size']])[0]
u=Uc(UC_ARCH_M68K,UC_MODE_BIG_ENDIAN);u.ctl_set_cpu_model(R.UC_CPU_M68K_ANY)
u.reg_write(R.UC_M68K_REG_SR,0x2700);u.mem_map(0x40000000,0x2000000);u.mem_write(BASE,main);u.mem_map(0x90000000,0x100000)
STOP=0x90000000;STACK=0x900ff000;OBJ=0x90010000;VT=0x90011000;DATA=0x90012000;VECTOR=0x90013000;ENTRIES=0x90014000;GET=0x90020000;NOTIFY=0x90020004;EVENT=0x90030000
notes=[]
def w(a,*v):u.mem_write(a,struct.pack('>'+str(len(v))+'I',*v))
def call(a,*v):
 w(STACK,STOP,*v);u.reg_write(R.UC_M68K_REG_A7,STACK);u.reg_write(R.UC_M68K_REG_SR,0x2700);u.emu_start(a,STOP,count=200000);assert u.reg_read(R.UC_M68K_REG_PC)==STOP;return u.reg_read(R.UC_M68K_REG_D0)
def hook(uc,a,size,data):
 sp=uc.reg_read(R.UC_M68K_REG_A7)
 if a==GET:uc.reg_write(R.UC_M68K_REG_D0,DATA)
 elif a==NOTIFY:notes.append(bytes(uc.mem_read(DATA,100)))
 else:return
 ret=struct.unpack('>I',uc.mem_read(sp,4))[0];uc.reg_write(R.UC_M68K_REG_A7,sp+4);uc.reg_write(R.UC_M68K_REG_PC,ret)
u.hook_add(UC_HOOK_CODE,hook);w(OBJ,VT);w(VT+40,GET);w(VT+16,NOTIFY)
# Actual PadEvent constructor and actual accessor: index1..6, press/release, velocity.
for pad in range(1,7):
 for down in [0,1]:
  call(0x40074072,EVENT,pad,down,93,1234,0)
  assert call(0x400740ca,EVENT)==pad
  assert (call(0x400740d4,EVENT)&255)==down
  assert (call(0x400740e4,EVENT)&255)==1-down
  assert call(0x400740f2,EVENT)==93
print('PASS: stock PadEvent constructor/accessors all6 pads press/release')
# Actual batch sound operation:10 words, preservation, one notification after all writes.
before=bytes((i*7+3)%256 for i in range(100));u.mem_write(DATA,before)
for i in range(10):
 w(ENTRIES+i*8,9+i);u.mem_write(ENTRIES+i*8+4,struct.pack('>H',i<<8))
w(VECTOR,ENTRIES,ENTRIES+80,ENTRIES+80)
call(0x4001416c,OBJ,VECTOR)
after=bytes(u.mem_read(DATA,100));expected=bytearray(before)
for i in range(10):expected[20+2*(9+i):22+2*(9+i)]=struct.pack('>H',i<<8)
assert after==bytes(expected);assert notes==[after]
print('PASS: stock sound batch writes slots9..18, preserves other80 bytes, notifies once after writes')
print('LIMIT: synthetic owner getter/notify; real observer propagation, machine transition and rollback unproven')
