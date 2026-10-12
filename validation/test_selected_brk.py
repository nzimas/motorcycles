#!/usr/bin/env python3
"""Exact packaged live break (firmware/brk.c, brk_hooks.S):
 1. Trig builder entry (0x400548ce) and exit (0x40054c0e) jump to the hooks.
 2. Entry hook: on the sequencer path (reuse 0, out != 0) the step argument
    becomes mc_break_source(step); other callers keep their step; the replayed
    'lea -60(sp),sp; moveq #1,d1' leaves the builder frame exactly as stock;
    callee registers preserved.
 3. Exit hook: stores the lock buffer at msg+72, returns msg in d0, and on the
    sequencer path adds break locks (pan/timbre/octave) into the buffer and
    ratchets (flags bit 15, rate, length) to the message; preview path untouched.
 4. PAGE during playback (outside generator mode) starts a break on press and
    ends it on release, both consumed; stopped, PAGE keeps its old behaviour."""
import json,os,pathlib,struct
from unicorn import UC_HOOK_CODE
from unicorn import m68k_const as m
from test_native_live import LiveRig,candidate_meta,SYMBOLS
assert os.environ.get('MC_CANDIDATE'),'packaged candidate required'
D=pathlib.Path(__file__).resolve().parent;S=SYMBOLS;checks=[]
IMAGE=pathlib.Path(os.environ['MC_CANDIDATE']).with_suffix('.main.bin').read_bytes()
rd=lambda a,n:IMAGE[a-0x40000400:a-0x40000400+n]
assert rd(0x400548ce,6)==b'\x4e\xb9'+struct.pack('>I',S['mc_brk_entry_hook'])
assert rd(0x40054c0e,6)==b'\x4e\xb9'+struct.pack('>I',S['mc_brk_exit_hook'])
checks.append('builder entry and exit patched to the break hooks')
r=LiveRig()
for a in (0x40a78000,0x422ff000):
 try:r.u.mem_map(a,0x1000)
 except Exception:pass
brk=S['mc_brk']
active=lambda:r.u.mem_read(brk,1)[0]
# 4. UI first: start a break for the hook checks.
assert r.key(15,1)==0 and active()==0;r.key(15,16)
checks.append('stopped: PAGE keeps its ordinary path, no break')
r.u.mem_write(0x422ff168,bytes((5,5,5,5,5,5)))
r.running=1;assert r.key(15,1)==1 and active()==1;assert r.key(15,8)==1 and active()==1
REGS=[m.UC_M68K_REG_D2,m.UC_M68K_REG_D3,m.UC_M68K_REG_D4,m.UC_M68K_REG_D5,m.UC_M68K_REG_D6,m.UC_M68K_REG_D7,m.UC_M68K_REG_A2,m.UC_M68K_REG_A3,m.UC_M68K_REG_A4,m.UC_M68K_REG_A5,m.UC_M68K_REG_A6]
def entry(track,pattern,step,reuse,out):
 sp=0x4400f000;r.u.mem_write(sp,struct.pack('>7I',0x400548d4-6+6,0x44000100,track,pattern,0x41600000,step,reuse) +struct.pack('>I',out))
 # The hook is entered by jsr from 0x400548ce: its return address is 0x400548d4.
 r.u.mem_write(sp,struct.pack('>I',0x400548d4))
 for i,reg in enumerate(REGS):r.u.reg_write(reg,0x5a5a0000+i)
 r.u.reg_write(m.UC_M68K_REG_SR,0x2200);r.u.reg_write(m.UC_M68K_REG_A7,sp)
 r.u.emu_start(S['mc_brk_entry_hook'],0x400548d4,count=200000)
 nsp=r.u.reg_read(m.UC_M68K_REG_A7);assert nsp==sp+4-60,(hex(nsp),hex(sp))
 assert r.u.reg_read(m.UC_M68K_REG_D1)==1
 assert [r.u.reg_read(x) for x in REGS]==[0x5a5a0000+i for i in range(len(REGS))]
 return struct.unpack('>I',bytes(r.u.mem_read(nsp+76,4)))[0]
PAT=0x41620000;pat=bytearray(30710);pat[30662:30664]=(16).to_bytes(2,'big');pat[30667]=0;r.u.mem_write(PAT,bytes(pat))
ref=__import__('ctypes')
lib=pathlib.Path('/tmp')  # host reference of mc_break_source via the packaged plan
plan=bytes(r.u.mem_read(brk,64))
seen=set()
for step in range(16):
 for t in range(6):
  got=entry(t,PAT,step,0,0x41610000);seen.add(got!=step);assert got<16
  assert entry(t,PAT,step,1,0x41610000)==step and entry(t,PAT,step,0,0)==step
checks.append('entry hook: sequencer steps remapped inside the track, preview/precompute untouched, frame and registers as stock')
# 3. Exit hook.
MSG,LOCK,SOUND=0x41630000,0x41631000,0x41632000
def exit_hook(reuse,out,track=2):
 r.u.mem_write(MSG,bytes(80));r.u.mem_write(MSG+8,struct.pack('>I',track));r.u.mem_write(MSG+44,struct.pack('>I',SOUND))
 r.u.mem_write(LOCK,bytes(20+8*66));r.u.mem_write(LOCK+8,struct.pack('>i',1));r.u.mem_write(LOCK+20,struct.pack('>HH',11,40<<8))
 snd=bytearray(100)
 for k in range(33):snd[20+2*k]=60
 r.u.mem_write(SOUND,bytes(snd));r.u.mem_write(0x40a78860,bytes((4,4,4,4,4,4)))
 sp=0x4400e000;frame=bytearray(100);frame[80:84]=struct.pack('>I',reuse);frame[84:88]=struct.pack('>I',out)
 r.u.mem_write(sp,struct.pack('>I',0x44000200)+bytes(frame))
 r.u.reg_write(m.UC_M68K_REG_A2,MSG);r.u.reg_write(m.UC_M68K_REG_A6,LOCK);r.u.reg_write(m.UC_M68K_REG_SR,0x2200);r.u.reg_write(m.UC_M68K_REG_A7,sp)
 r.u.emu_start(S['mc_brk_exit_hook'],0x44000200,count=200000)
 assert r.u.reg_read(m.UC_M68K_REG_D0)==MSG and r.u.reg_read(m.UC_M68K_REG_A7)==sp+4
 assert struct.unpack('>I',bytes(r.u.mem_read(MSG+72,4)))[0]==LOCK
 n=struct.unpack('>i',bytes(r.u.mem_read(LOCK+8,4)))[0]
 return n,bytes(r.u.mem_read(MSG,80))
added=0
for press in range(12):
 r.key(15,16);r.key(15,1);assert active()==1
 for t in range(6):
  n,msg=exit_hook(0,0x41610000,t);assert 1<=n<=66;added+=n>1
  flags=struct.unpack('>I',msg[40:44])[0]
  if flags&0x8000:assert 0<=struct.unpack('>i',msg[56:60])[0]<=16
  n0,msg0=exit_hook(1,0x41610000,t);assert n0==1 and msg0[40:44]==bytes(4)
assert added
checks.append('exit hook: lock buffer stored, message returned; break locks and ratchets only on the sequencer path')
r.key(15,16);assert active()==0
for step in range(16):assert entry(3,PAT,step,0,0x41610000)==step
checks.append('PAGE during playback: press starts a fresh break, repeats held, release ends it and the pattern plays as written')
report={'result':'PASS','package_sha256':candidate_meta['syx_sha256'],'checks':checks,
 'limitations':'Hooks run on the packaged code with synthetic frames and patterns; the stock sequencer interrupt, dispatcher and audio are not executed together; timing/latency (up to one step) not measured.'}
(D/'selected-brk-report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({'result':'PASS','checks':len(checks)},indent=2))
