"""Freestanding ColdFire sequence staging parity, synthetic RAM, no publication."""
from pathlib import Path
import ctypes,struct,json,hashlib
from unicorn import Uc,UC_ARCH_M68K,UC_MODE_BIG_ENDIAN,UC_HOOK_MEM_WRITE
from unicorn.m68k_const import *
from elftools.elf.elffile import ELFFile
ROOT=Path(__file__).resolve().parent
cpu=Uc(UC_ARCH_M68K,UC_MODE_BIG_ENDIAN);cpu.ctl_set_cpu_model(UC_CPU_M68K_CFV4E)
CODE=0x20000000;DATA=0x21000000;STACK=0x22000000;STOP=0x23000000
for base,size in ((CODE,65536),(DATA,65536),(STACK,65536),(STOP,4096)):cpu.mem_map(base,size)
with (ROOT/'sequence-target.elf').open('rb') as f:
 elf=ELFFile(f)
 for seg in elf.iter_segments():
  if seg['p_type']=='PT_LOAD':cpu.mem_write(seg['p_vaddr'],seg.data())
 symbols={s.name:s['st_value'] for s in elf.get_section_by_name('.symtab').iter_symbols()}
PHRASE=DATA;CODES=DATA+0x200;OLD=DATA+0x1000;TRACK=DATA+0x2000;LOCK=DATA+0x3000
minimum_sp=STACK+0xff00
saved=[UC_M68K_REG_D2,UC_M68K_REG_D3,UC_M68K_REG_D4,UC_M68K_REG_D5,UC_M68K_REG_D6,UC_M68K_REG_D7,UC_M68K_REG_A2,UC_M68K_REG_A3,UC_M68K_REG_A4,UC_M68K_REG_A5,UC_M68K_REG_A6]
def writes(cpu,access,address,size,value,_):
 global minimum_sp
 assert TRACK<=address and address+size<=TRACK+722 or LOCK<=address and address+size<=LOCK+4385 or STACK<=address and address+size<=STACK+65536,hex(address)
 if STACK<=address<STACK+65536:minimum_sp=min(minimum_sp,address)
cpu.hook_add(UC_HOOK_MEM_WRITE,writes)
def call(args):
 sp=STACK+0xff00;cpu.mem_write(sp,struct.pack('>7I',STOP,*args));cpu.reg_write(UC_M68K_REG_SR,0x2700);cpu.reg_write(UC_M68K_REG_A7,sp)
 for i,r in enumerate(saved):cpu.reg_write(r,0x12340000+i)
 cpu.emu_start(symbols['mc_stage_sequence'],STOP,count=500000)
 assert cpu.reg_read(UC_M68K_REG_PC)==STOP and cpu.reg_read(UC_M68K_REG_A7)==sp+4
 for i,r in enumerate(saved):assert cpu.reg_read(r)==0x12340000+i
 return cpu.reg_read(UC_M68K_REG_D0)
lib=ctypes.CDLL(str(ROOT/'sequence-host.dylib'));lib.mc_stage_sequence.argtypes=[ctypes.c_void_p]*5+[ctypes.c_uint]
codes=bytes([0,14,30,38,46,50,54,58,62,64,66,68,70]);cases=0
for seed in range(64):
 length=(16,32,48,64)[seed%4]
 phrase=bytes([length,seed%6])+b''.join(bytes([int(i<length and (i+seed)%3==0),40+(i+seed)%57,48+(i*7+seed)%59,1+(i+seed)%12]) for i in range(64))
 old=bytes((i*7+seed*19)&255 for i in range(722))
 pbuf=ctypes.create_string_buffer(phrase);obuf=ctypes.create_string_buffer(old);tbuf=ctypes.create_string_buffer(722);lbuf=ctypes.create_string_buffer(4385);cbuf=ctypes.create_string_buffer(codes)
 assert lib.mc_stage_sequence(pbuf,obuf,tbuf,lbuf,cbuf,seed%2)==1
 cpu.mem_write(PHRASE,phrase);cpu.mem_write(CODES,codes);cpu.mem_write(OLD,old);cpu.mem_write(TRACK,b'\xa5'*722);cpu.mem_write(LOCK,b'\xa5'*4385)
 assert call([PHRASE,OLD,TRACK,LOCK,CODES,seed%2])==1
 assert bytes(cpu.mem_read(TRACK,722))==tbuf.raw and bytes(cpu.mem_read(LOCK,4385))==lbuf.raw
 cases+=1
# Input alias / pointer wraps / event bounds must reject before modifying scratch.
normal=[PHRASE,OLD,TRACK,LOCK,CODES,0]
invalid=[]
for arg,value in ((0,0xfffffff0),(1,0xfffffff0),(2,0xfffffff0),(3,0xfffffff0),(4,0xfffffff9),(1,TRACK+1),(1,LOCK),(0,TRACK),(4,LOCK)):
 args=list(normal);args[arg]=value;invalid.append(args)
for args in invalid:
 cpu.mem_write(TRACK,b'\xa5'*722);cpu.mem_write(LOCK,b'\xa5'*4385)
 assert call(args)==0
 assert bytes(cpu.mem_read(TRACK,722))==b'\xa5'*722 and bytes(cpu.mem_read(LOCK,4385))==b'\xa5'*4385
report={'cpu':'Unicorn CFV4E','target':'gcc54415','host_target_exact_cases':cases,'invalid_alias_wrap_cases':len(invalid),'callee_registers_preserved':True,'bounded_writes':True,'max_observed_stack_bytes':STACK+0xff00-minimum_sp,'elf_sha256':hashlib.sha256((ROOT/'sequence-target.elf').read_bytes()).hexdigest(),'staging_source_sha256':hashlib.sha256((ROOT/'sequence_encoding.c').read_bytes()).hexdigest(),'duration_timing_proved':False,'owner_binding_proved':False,'publication_proved':False,'instrument_access':False}
(ROOT/'sequence-target-report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
