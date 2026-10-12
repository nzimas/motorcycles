#!/usr/bin/env python3
"""Exact packaged Phrase/Pattern commits write parameter locks the stock OS can
keep: per-step counts and slot-used flags are exact, the pattern stays within
the saver's 79 records, and the stock saver (4005b95c) plus loader (4005b766)
round-trip every committed lock word, count and flag unchanged."""
import json,os,pathlib,struct
from unicorn import Uc,UC_ARCH_M68K,UC_MODE_BIG_ENDIAN
from unicorn import m68k_const as m
from test_native_live import LiveRig,PATTERN,PLAN,REQUEST,candidate_meta
assert os.environ.get('MC_CANDIDATE'),'packaged candidate required'
D=pathlib.Path(__file__).resolve().parent;ROOT=D.parent
L=6*4385
stock=Uc(UC_ARCH_M68K,UC_MODE_BIG_ENDIAN);stock.ctl_set_cpu_model(m.UC_CPU_M68K_CFV4E)
stock.mem_map(0x40000000,0x2000000);stock.mem_map(0x44000000,0x100000)
stock.mem_write(0x40000400,(ROOT/'adapter/stock-main.local.bin').read_bytes())
DATA,FILE,SP,STOP=0x41001000,0x44010000,0x4400ff00,0x44000000
def call(addr,*args):
 stock.mem_write(SP,struct.pack('>I'+'I'*len(args),STOP,*args));stock.reg_write(m.UC_M68K_REG_SR,0x2700);stock.reg_write(m.UC_M68K_REG_A7,SP)
 stock.emu_start(addr,STOP,count=5000000);assert stock.reg_read(m.UC_M68K_REG_PC)==STOP
def clean_locks():
 b=bytearray(b'\xff'*L)
 for t in range(6):
  for s in range(64):b[4385*t+68*s+66:4385*t+68*s+68]=b'\0\0'
  b[4385*t+4352:4385*t+4385]=bytes(33)
 return bytes(b)
def consistent(b):
 pairs=0
 for t in range(6):
  used=set()
  for s in range(64):
   n=0
   for k in range(33):
    v=struct.unpack_from('>H',b,4385*t+68*s+2*k)[0]
    if v!=0xffff:assert v<0x8000 and 1<=k<=22 and k not in (4,9),(t,s,k,hex(v));n+=1;used.add(k)
   assert struct.unpack_from('>H',b,4385*t+68*s+66)[0]==n,(t,s)
  assert [k for k in range(33) if b[4385*t+4352+k]]==sorted(used),t
  pairs+=len(used)
 return pairs
checks=[];total_pairs=0;locked_steps=0
for g in (2,3):
 for path in range(4):
  for brut in (0,100):
   for seed in (3,17,29):
    r=LiveRig();r.u.mem_write(PATTERN+4332,clean_locks())
    q=bytes((g,path,4,70,brut,63,0,0))+struct.pack('>I',seed*104729+path*7+g)+bytes((63,0,1,2,3,4,5,32))+bytes(60)+bytes((60,)*6+(63,0))
    r.u.mem_write(REQUEST,q);assert r.call('mc_generate',REQUEST,PLAN)==1
    assert r.call('mc_commit_live',0,PLAN,REQUEST)==1,(g,path,brut,seed)
    committed=bytes(r.u.mem_read(PATTERN+4332,L));pairs=consistent(committed);assert pairs<=79,pairs;total_pairs+=pairs
    locked_steps+=sum(struct.unpack_from('>H',committed,4385*t+68*s+66)[0]>0 for t in range(6) for s in range(64))
    stock.mem_write(DATA,committed);stock.mem_write(FILE,bytes(80*130+64));call(0x4005b95c,FILE,DATA,0)
    stock.mem_write(DATA,bytes(L));call(0x4005b766,DATA,FILE)
    assert bytes(stock.mem_read(DATA,L))==committed,(g,path,brut,seed)
    checks.append('generator%d path%d brutality%d seed%d: %d pairs saved and reloaded exactly'%(g,path,brut,seed,pairs))
assert total_pairs and locked_steps
report={'result':'PASS','package_sha256':candidate_meta['syx_sha256'],'checks':checks,'locked_steps':locked_steps,
 'limitations':'Exact packaged generator/commit and stock lock saver/loader on the committed lock area; synthetic owner tree; not a filesystem write or power cycle.'}
(D/'selected-lock-persistence-report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({'result':'PASS','cases':len(checks),'locked_steps':locked_steps,'pairs':total_pairs},indent=2))
