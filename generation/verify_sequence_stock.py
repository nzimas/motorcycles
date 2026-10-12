"""Compare staging reset to the actual stock 722B/4385B initializer, synthetic RAM."""
from pathlib import Path
import ctypes,struct,json,hashlib,time
from unicorn import Uc,UC_ARCH_M68K,UC_MODE_BIG_ENDIAN,UC_HOOK_CODE
from unicorn import m68k_const as R
ROOT=Path(__file__).resolve().parent
main=(ROOT.parent/'adapter/stock-main.local.bin').read_bytes()
lib=ctypes.CDLL(str(ROOT/'sequence-host.dylib'));lib.mc_stage_sequence.argtypes=[ctypes.c_void_p]*5+[ctypes.c_uint]
u=Uc(UC_ARCH_M68K,UC_MODE_BIG_ENDIAN);u.ctl_set_cpu_model(R.UC_CPU_M68K_ANY)
u.mem_map(0x40000000,0x2000000);u.mem_write(0x40000400,main);u.mem_map(0x90000000,0x100000)
STOP=0x90000000;STACK=0x900ff000;TRACK=0x90010000;LOCK=0x90011000
OBJ=0x90030000;VT=0x90031000;GET=0x90032000;NOTIFY=GET+4
u.mem_write(OBJ,struct.pack('>I',VT));u.mem_write(VT+40,struct.pack('>I',GET));u.mem_write(VT+16,struct.pack('>I',NOTIFY))
def hook(cpu,a,size,_):
 if a not in (GET,NOTIFY):return
 if a==GET:cpu.reg_write(R.UC_M68K_REG_D0,TRACK)
 sp=cpu.reg_read(R.UC_M68K_REG_A7);ret=struct.unpack('>I',cpu.mem_read(sp,4))[0];cpu.reg_write(R.UC_M68K_REG_A7,sp+4);cpu.reg_write(R.UC_M68K_REG_PC,ret)
u.hook_add(UC_HOOK_CODE,hook)
def call(fn,*args):
 u.mem_write(STACK,struct.pack('>'+'I'*(len(args)+1),STOP,*args));u.reg_write(R.UC_M68K_REG_SR,0x2700);u.reg_write(R.UC_M68K_REG_A7,STACK);u.emu_start(fn,STOP,count=200000);assert u.reg_read(R.UC_M68K_REG_PC)==STOP;return u.reg_read(R.UC_M68K_REG_D0)
for value in (-500,-1,0,1,99,100,101,1000):
 call(0x40016d3e,OBJ,value&0xffffffff)
 assert bytes(u.mem_read(TRACK+719,1))==bytes([max(0,min(100,value))])
for seed in range(64):
 u.mem_write(0x40149308,struct.pack('>II',seed*10007,seed*90013))
 before=bytes(u.mem_read(0x40149308,8));assert call(0x400913a8,100)==1
 assert bytes(u.mem_read(0x40149308,8))==before
 assert call(0x400913a8,0)==0
start=time.monotonic()
for seed in range(32):
 old=bytes((i*7+seed*19)&255 for i in range(722))
 u.mem_write(TRACK,old);u.mem_write(LOCK,b'\xa5'*4385)
 u.mem_write(STACK,struct.pack('>III',STOP,TRACK,LOCK));u.reg_write(R.UC_M68K_REG_SR,0x2700);u.reg_write(R.UC_M68K_REG_A7,STACK)
 u.emu_start(0x40061480,STOP,count=200000)
 assert u.reg_read(R.UC_M68K_REG_PC)==STOP
 call(0x40016d3e,OBJ,100)
 oldbuf=ctypes.create_string_buffer(old);track=ctypes.create_string_buffer(722);locks=ctypes.create_string_buffer(4385);phrase=ctypes.create_string_buffer(258);codes=ctypes.create_string_buffer(13)
 assert lib.mc_stage_sequence(phrase,oldbuf,track,locks,codes,0)==1
 assert bytes(u.mem_read(TRACK,722))==track.raw
 assert bytes(u.mem_read(LOCK,4385))==locks.raw
report={'stock_initializer':'0x40061480','canonical_track_bytes':722,'canonical_lock_bytes':4385,'host_stock_exact_reset_and_chance_cases':32,'chance_100_always_cases':64,'chance_0_never_cases':64,'stock_chance_setter_clamp_cases':8,'owner_binding_validated':False,'publication_validated':False,'duration_code_timing_validated':False,'staging_source_sha256':hashlib.sha256((ROOT/'sequence_encoding.c').read_bytes()).hexdigest(),'elapsed_seconds':round(time.monotonic()-start,3),'instrument_access':False}
(ROOT/'sequence-stock-report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
