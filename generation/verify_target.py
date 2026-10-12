"""Synthetic isolated ColdFire core oracle. No firmware or instrument access."""
from pathlib import Path
import ctypes, struct, hashlib, json, time
from unicorn import Uc, UC_ARCH_M68K, UC_MODE_BIG_ENDIAN, UC_HOOK_MEM_WRITE
from unicorn.m68k_const import *
from elftools.elf.elffile import ELFFile
ROOT=Path(__file__).resolve().parent
class Sound(ctypes.Structure):
    _fields_=[(n,ctypes.c_uint8) for n in ('machine','pitch','color','shape','sweep','contour','punch','gate','finetune','decay')]
class Request(ctypes.Structure):
    _fields_=[(n,ctypes.c_uint8) for n in ('generator','path','target','density','brutality','machine_mask')]+[('source_sound_valid_mask',ctypes.c_uint8),('music',ctypes.c_uint8),('seed',ctypes.c_uint32),('machine_valid_mask',ctypes.c_uint8),('current_machine',ctypes.c_uint8*6),('phrase_length',ctypes.c_uint8),('current_sound',Sound*6),('default_note',ctypes.c_uint8*6),('note_valid_mask',ctypes.c_uint8),('locks_elsewhere',ctypes.c_uint8)]
class Event(ctypes.Structure):
    _fields_=[(n,ctypes.c_uint8) for n in ('on','note','velocity','length')]+[('micro',ctypes.c_int8),('retrig',ctypes.c_uint8),('retrig_length',ctypes.c_uint8),('retrig_fade',ctypes.c_int8)]+[(n,ctypes.c_uint8) for n in ('condition','trigless','lock_count','reserved')]+[('lock_k',ctypes.c_uint8*4),('lock_v',ctypes.c_uint8*4)]
class SoundExt(ctypes.Structure):
    _fields_=[(n,ctypes.c_uint8) for n in ('lfo_speed','lfo_mult','lfo_fade','lfo_dest','lfo_wave','lfo_phase','lfo_mode','lfo_depth','volume','delay','reverb','pan')]
class Phrase(ctypes.Structure):
    _fields_=[('length',ctypes.c_uint8),('role',ctypes.c_uint8),('step',Event*64)]
class Plan(ctypes.Structure):
    _fields_=[('sound',Sound*6),('phrase',Phrase*6),('sound_mask',ctypes.c_uint8),('sequence_mask',ctypes.c_uint8),('tempo',ctypes.c_uint16),('extra',SoundExt*6)]
assert ctypes.sizeof(Request)==88 and ctypes.sizeof(Plan)==7828
host=ctypes.CDLL(str(ROOT/'generation-host.dylib'))
host.mc_generate.argtypes=[ctypes.POINTER(Request),ctypes.POINTER(Plan)]
host.mc_generate.restype=ctypes.c_int
host.mc_encode_engine.argtypes=[ctypes.POINTER(Sound),ctypes.POINTER(ctypes.c_uint8),ctypes.c_uint]
host.mc_encode_engine.restype=ctypes.c_int
cpu=Uc(UC_ARCH_M68K,UC_MODE_BIG_ENDIAN);cpu.ctl_set_cpu_model(UC_CPU_M68K_CFV4E)
CODE=0x20000000;DATA=0x21000000;STACK=0x22000000;STOP=0x23000000
for addr,size in ((CODE,0x10000),(DATA,0x10000),(STACK,0x10000),(STOP,0x1000)):cpu.mem_map(addr,size)
with (ROOT/'generation-target.elf').open('rb') as f:
    elf=ELFFile(f)
    for segment in elf.iter_segments():
        if segment['p_type']=='PT_LOAD':cpu.mem_write(segment['p_vaddr'],segment.data())
    symbols={s.name:s['st_value'] for s in elf.get_section_by_name('.symtab').iter_symbols()}
qptr=DATA;pptr=DATA+0x1000
writes=0
min_sp=STACK+0xff00
def written(cpu,access,addr,size,value,_):
    global writes,min_sp
    assert (pptr<=addr and addr+size<=pptr+1612) or (STACK<=addr and addr+size<=STACK+0x10000),hex(addr)
    writes+=1
    if STACK<=addr<STACK+0x10000:min_sp=min(min_sp,addr)
cpu.hook_add(UC_HOOK_MEM_WRITE,written)
def call(name,args):
    sp=STACK+0xff00;cpu.reg_write(UC_M68K_REG_SR,0x2700);cpu.reg_write(UC_M68K_REG_A7,sp)
    cpu.mem_write(sp,struct.pack('>'+ 'I'*(len(args)+1),STOP,*args))
    saved=[UC_M68K_REG_D2,UC_M68K_REG_D3,UC_M68K_REG_D4,UC_M68K_REG_D5,UC_M68K_REG_D6,UC_M68K_REG_D7,UC_M68K_REG_A2,UC_M68K_REG_A3,UC_M68K_REG_A4,UC_M68K_REG_A5,UC_M68K_REG_A6]
    for i,reg in enumerate(saved):cpu.reg_write(reg,0x12340000+i)
    cpu.emu_start(symbols[name],STOP,count=500000)
    assert cpu.reg_read(UC_M68K_REG_PC)==STOP,'budget exhausted'
    assert cpu.reg_read(UC_M68K_REG_A7)==sp+4,'stack corrupted'
    for i,reg in enumerate(saved):assert cpu.reg_read(reg)==0x12340000+i,'ABI register clobber'
    return cpu.reg_read(UC_M68K_REG_D0)
start=time.monotonic();cases=0;unique={};topology_unique={}
for g in range(4):
 for path in range(4):
  seen=set();topologies=set()
  for seed in range(1,129):
   q=Request(g,path,3,30,35,63,0,0,seed,63,(ctypes.c_uint8*6)(0,1,2,3,4,5))
   p=Plan();assert host.mc_generate(ctypes.byref(q),ctypes.byref(p))==1
   # six byte prefix, two padding bytes, BE seed, seven metadata bytes and optional phrase length.
   raw=struct.pack('>8BI8B',q.generator,q.path,q.target,q.density,q.brutality,q.machine_mask,q.source_sound_valid_mask,q.music,q.seed,q.machine_valid_mask,*q.current_machine,q.phrase_length)+bytes(q.current_sound)+bytes(q.default_note)+bytes([q.note_valid_mask,q.locks_elsewhere])
   cpu.mem_write(qptr,raw);cpu.mem_write(pptr,b'\xa5'*1612)
   assert call('mc_generate',[qptr,pptr])==1
   target=bytes(cpu.mem_read(pptr,1612));expected=bytes(p)[:-2]+struct.pack('>H',p.tempo)
   assert target==expected,(g,path,seed)
   seen.add(hashlib.sha256(target).hexdigest());cases+=1
   if g>=2:topologies.add(bytes([line.length for line in p.phrase])+bytes(event.on for line in p.phrase for event in line.step))
  assert len(seen)==128
  unique[f'{g}:{path}']=len(seen)
  if g>=2:topology_unique[f'{g}:{path}']=len(topologies)
phrase_length_cases=0
for length in range(1,65):
 q=Request(2,3,3,100,35,63,0,0,17,63,(ctypes.c_uint8*6)(0,1,2,3,4,5),length)
 p=Plan();assert host.mc_generate(ctypes.byref(q),ctypes.byref(p))==1
 raw=struct.pack('>8BI8B',q.generator,q.path,q.target,q.density,q.brutality,q.machine_mask,q.source_sound_valid_mask,q.music,q.seed,q.machine_valid_mask,*q.current_machine,q.phrase_length)+bytes(q.current_sound)+bytes(q.default_note)+bytes([q.note_valid_mask,q.locks_elsewhere])
 cpu.mem_write(qptr,raw);cpu.mem_write(pptr,b'\xa5'*1612)
 assert call('mc_generate',[qptr,pptr])==1
 assert bytes(cpu.mem_read(pptr,1612))==bytes(p)[:-2]+struct.pack('>H',p.tempo)
 assert p.phrase[3].length==length
 phrase_length_cases+=1
raw=bytearray(raw);raw[19]=65;cpu.mem_write(qptr,bytes(raw));cpu.mem_write(pptr,b'\xa5'*1612)
assert call('mc_generate',[qptr,pptr])==0 and bytes(cpu.mem_read(pptr,1612))==b'\xa5'*1612
extension_cases=0
for machine in (6,7):
 for g in range(4):
  for path in range(4):
   for seed in range(1,17):
    q=Request();q.generator=g;q.path=path;q.target=3;q.density=30;q.brutality=35;q.machine_mask=255;q.seed=seed;q.machine_valid_mask=63;q.source_sound_valid_mask=63
    for t in range(6):
     q.current_machine[t]=machine;q.current_sound[t]=Sound(machine,64,19,34 if machine==7 else 113,87,21,1,0,64,45)
    p=Plan();assert host.mc_generate(ctypes.byref(q),ctypes.byref(p))==1
    raw=struct.pack('>8BI8B',q.generator,q.path,q.target,q.density,q.brutality,q.machine_mask,q.source_sound_valid_mask,q.music,q.seed,q.machine_valid_mask,*q.current_machine,q.phrase_length)+bytes(q.current_sound)+bytes(q.default_note)+bytes([q.note_valid_mask,q.locks_elsewhere])
    cpu.mem_write(qptr,raw);cpu.mem_write(pptr,b'\xa5'*1612)
    assert call('mc_generate',[qptr,pptr])==1
    assert bytes(cpu.mem_read(pptr,1612))==bytes(p)[:-2]+struct.pack('>H',p.tempo)
    extension_cases+=1
music_cases=0
for root in range(12):
 for scale in range(4):
  for g in range(4):
   for path in range(4):
    q=Request();q.generator=g;q.path=path;q.target=3;q.density=30;q.brutality=55;q.machine_mask=255;q.seed=31;q.machine_valid_mask=63;q.source_sound_valid_mask=63;q.music=128|(scale<<4)|root;q.note_valid_mask=63
    for t in range(6):
     q.current_machine[t]=4;q.current_sound[t]=Sound(4,73,45,40,25,20,0,0,64,45);q.default_note[t]=52+t*5
    p=Plan();assert host.mc_generate(ctypes.byref(q),ctypes.byref(p))==1
    raw=struct.pack('>8BI8B',q.generator,q.path,q.target,q.density,q.brutality,q.machine_mask,q.source_sound_valid_mask,q.music,q.seed,q.machine_valid_mask,*q.current_machine,q.phrase_length)+bytes(q.current_sound)+bytes(q.default_note)+bytes([q.note_valid_mask,q.locks_elsewhere])
    cpu.mem_write(qptr,raw);cpu.mem_write(pptr,b'\xa5'*1612)
    assert call('mc_generate',[qptr,pptr])==1
    assert bytes(cpu.mem_read(pptr,1612))==bytes(p)[:-2]+struct.pack('>H',p.tempo)
    music_cases+=1
music_invalid_cases=0
for kind in range(6):
 q=Request();q.generator=0;q.path=3;q.target=3;q.density=30;q.brutality=55;q.machine_mask=1;q.seed=31
 if kind==0:q.music=1
 elif kind==1:q.music=0xc0
 elif kind==2:q.music=0x8c
 elif kind==3:q.note_valid_mask=64
 else:q.note_valid_mask=8;q.default_note[3]=128 if kind==4 else 127
 raw=struct.pack('>8BI8B',q.generator,q.path,q.target,q.density,q.brutality,q.machine_mask,q.source_sound_valid_mask,q.music,q.seed,q.machine_valid_mask,*q.current_machine,q.phrase_length)+bytes(q.current_sound)+bytes(q.default_note)+bytes([q.note_valid_mask,q.locks_elsewhere])
 p=Plan();ctypes.memset(ctypes.byref(p),0xa5,1612)
 assert host.mc_generate(ctypes.byref(q),ctypes.byref(p))==0 and bytes(p)==b'\xa5'*1612
 cpu.mem_write(qptr,raw);cpu.mem_write(pptr,b'\xa5'*1612)
 assert call('mc_generate',[qptr,pptr])==0 and bytes(cpu.mem_read(pptr,1612))==b'\xa5'*1612
 music_invalid_cases+=1
engine_cases=0
for machine in range(8):
 for pitch in (40,64,88):
  sound=Sound(machine,pitch,127,1,64,100,1,0,64,127)
  buffer=(ctypes.c_uint8*38)(*[0xa5]*38)
  assert host.mc_encode_engine(ctypes.byref(sound),buffer,38)==1
  cpu.mem_write(qptr,bytes(sound));cpu.mem_write(pptr,b'\xa5'*38)
  assert call('mc_encode_engine',[qptr,pptr,38])==1
  assert bytes(cpu.mem_read(pptr,38))==bytes(buffer)
  engine_cases+=1
# Invalid request must leave every output byte intact on the target CPU.
raw=bytearray(raw);raw[2]=6;cpu.mem_write(qptr,bytes(raw));cpu.mem_write(pptr,b'\xa5'*1612)
assert call('mc_generate',[qptr,pptr])==0 and bytes(cpu.mem_read(pptr,1612))==b'\xa5'*1612
report={'cpu_model':'Unicorn CFV4E','target':'gcc -mcpu=54415','host_target_exact_cases':cases,'unique_per_generator_path':unique,'distinct_sequence_onset_topologies':topology_unique,'engine_encoder_exact_cases':engine_cases,'music_config_exact_cases':music_cases,'music_invalid_atomic_cases':music_invalid_cases,'extension_metadata_exact_cases':extension_cases,'phrase_length_exact_cases':phrase_length_cases,'phrase_length_invalid_atomic':True,'invalid_request_atomic':True,'protected_write_ranges':True,'callee_saved_registers_preserved':True,'max_observed_stack_bytes':STACK+0xff00-min_sp,'elf_sha256':hashlib.sha256((ROOT/'generation-target.elf').read_bytes()).hexdigest(),'elapsed_seconds':round(time.monotonic()-start,3),'instrument_access':False,'firmware_package_validation':False,'audio_validation':False}
(ROOT/'target-report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
