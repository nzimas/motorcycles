#!/usr/bin/env python3
"""Selected-package machine staging/publication; synthetic OS owner fixtures."""
import os,pathlib,json,struct,hashlib
from unicorn import UC_HOOK_MEM_WRITE,UC_HOOK_CODE
ROOTDIR=pathlib.Path(__file__).resolve().parents[1];C=ROOTDIR/'build/selected-native/ModelCycles-generative-selected'
os.environ['MC_CANDIDATE']=str(C);os.environ['MC_NATIVE_PAYLOAD']=str(C.with_suffix('.payload.bin'))
from test_native_live import LiveRig,ROOT,SOUND,PATTERN,PLAN,REQUEST,checks as basechecks
checks=[]
def watch(r):
 bad=[];state=[True]
 def code(u,a,n,d):
  if a==r.symbols['mc_irq_save']:state[0]=False
 def write(u,t,a,n,v,d):
  if state[0] and (SOUND<=a<SOUND+600 or ROOT<=a<ROOT+620 or 0x401ab6f4<=a<0x401ab6fa):bad.append((hex(a),n))
 r.u.hook_add(UC_HOOK_CODE,code);r.u.hook_add(UC_HOOK_MEM_WRITE,write);return bad
for source in range(7):
 for dest in [6,0,5]:  # MACRO is machine 6; no Model-TG Sampler
  r=LiveRig();address=SOUND+400;r.u.mem_write(address+38,bytes((source,0)));before=bytes(r.u.mem_read(address,100));scratch=0x411a0000;r.u.mem_write(scratch,before)
  bad=watch(r);assert r.call('mc_fw_stage_machine',ROOT+212+68*4,scratch,dest)==1,(source,dest)
  assert bytes(r.u.mem_read(address,100))==before and not bad,(source,dest,bad)
  after=bytes(r.u.mem_read(scratch,100));assert after[38:40]==bytes((dest,0))
  if source==dest:assert after==before
  checks.append('private machine%d→%d scratch-only selected defaults'%(source,dest))
for g in [2,3]:
 for path in range(4):
  r=LiveRig();machines=[6,0,1,6,2,3]
  for i,machine in enumerate(machines):r.u.mem_write(SOUND+100*i+38,bytes((machine,0)))
  q=bytes((g,path,4,35,50,255,0,0))+struct.pack('>I',123+path)+bytes((63,*machines,32))+bytes(60)+bytes((60,60,60,60,60,60,63,0));r.u.mem_write(REQUEST,q)
  assert r.call('mc_generate',REQUEST,PLAN)==1;old=r.capture();bad=watch(r);assert r.call('mc_commit_live',0,PLAN,REQUEST)==1
  new=r.capture()[1]
  # Phrase preserves sounds; Pattern changes only VOLUME+DIST (k19) to 127.
  assert all(new[i]==old[1][i] if g==2 else new[i][:58]+new[i][60:]==old[1][i][:58]+old[1][i][60:] and new[i][58:60]==b'\x7f\x00' for i in range(6)) and not bad
  checks.append('MACRO generator%d path%d preserves sounds (Pattern: volume 127 only)'%(g,path))
manifest=json.loads(C.with_suffix('.manifest.json').read_text());image=C.with_suffix('.main.bin').read_bytes()
# Standalone MACRO: its boot stub clears and fills 0x43000000..macro_end; the
# native runtime starts above it and stays below the stack at 0x48000000.
stub=image[0x4016cae8-0x40000400:0x4016cae8-0x40000400+12];assert stub[:8]==bytes.fromhex('43f943000000203c')
macro_end=0x43000000+4*int.from_bytes(stub[8:12],'big')
assert 0x43000000<macro_end<=manifest['payload_start']<manifest['payload_memory_end']<=manifest['runtime_limit']<0x48000000
report={'memory_nonoverlap':{'bss_clear_end':'0x423380b0','macro_start':'0x43000000','macro_end':hex(macro_end),'native_start':hex(manifest['payload_start']),'native_memory_end':hex(manifest['payload_memory_end']),'stack_top':'0x48000000',
 'reason':'No stock code addresses memory between the BSS end and the stack (Modded-Cycles notes/17); MACRO runs at 0x43000000 on hardware.'},'result':'PASS','baseline_live_checks':len(basechecks),'checks':checks,
 'package_sha256':hashlib.sha256(C.with_suffix('.syx').read_bytes()).hexdigest(),
 'limitations':'Exact selected packaged MAIN/native payload and actual MACRO bootstrap copy. Synthetic owner tree, stock descriptor rows, transport/heap and track/global notification spies. No audio render, full boot or ISR deadline proof.'}
(ROOTDIR/'validation/selected-machines-report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
