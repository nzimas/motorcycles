#!/usr/bin/env python3
"""Execute exact ColdFire interface ELF payload; compare canonical host snapshots."""
import hashlib,json,pathlib
from unicorn import Uc,UC_ARCH_M68K,UC_MODE_BIG_ENDIAN,UC_HOOK_MEM_WRITE,UC_HOOK_CODE
from unicorn import m68k_const as m
D=pathlib.Path(__file__).resolve().parent
symbols={line.split()[2]:int(line.split()[0],16) for line in (D/'interface-target-symbols.txt').read_text().splitlines()}
uc=Uc(UC_ARCH_M68K,UC_MODE_BIG_ENDIAN);uc.ctl_set_cpu_model(m.UC_CPU_M68K_CFV4E)
uc.mem_map(0x43000000,0x10000);uc.mem_map(0x43100000,0x10000);uc.mem_map(0x44000000,0x10000);uc.mem_map(0x45000000,0x1000)
uc.mem_write(0x43000000,(D/'interface-target.bin').read_bytes());stop=0x45000000;sp=0x4400fff0
uc.reg_write(m.UC_M68K_REG_SR,0x2700);uc.reg_write(m.UC_M68K_REG_A7,sp);uc.mem_write(sp,stop.to_bytes(4,'big'))
low_sp=sp;instructions=0;writes=0;bad=[]
def code(u,a,n,d):
 global low_sp,instructions
 low_sp=min(low_sp,u.reg_read(m.UC_M68K_REG_A7));instructions+=1
 if instructions>2000000:raise RuntimeError('instruction budget exceeded')
def write(u,access,a,n,v,d):
 global writes
 writes+=1
 if not (0x44000000<=a and a+n<=sp+4 or symbols['request']<=a and a+n<=symbols['_end']):bad.append((a,n))
uc.hook_add(UC_HOOK_CODE,code);uc.hook_add(UC_HOOK_MEM_WRITE,write)
uc.emu_start(symbols['run_suite'],stop,count=2000000)
def u32(name):return int.from_bytes(uc.mem_read(symbols[name],4),'big')
COUNT,WIDTH=u32('snapshot_count'),u32('snapshot_width')
actual=bytes(uc.mem_read(symbols['output'],COUNT*WIDTH));expected=(D/'interface-target-host.bin').read_bytes()
# Semantic spot checks of the (shared) snapshot against the generator UI spec.
row=lambda k:expected[k*WIDTH:(k+1)*WIDTH]
PUBS,ACK,STATE,LEVEL,REQ,AUD,CONSUMED,LEDS=40,12,1,13,34,20,0,41
leds=lambda k:int.from_bytes(row(k)[LEDS:LEDS+2],'little')
assert len(expected)==COUNT*WIDTH==64*43
assert row(3)[STATE]==2 and row(5)[LEVEL]==5,'latched READY, step5 sets level 5'
assert row(7)[PUBS]==0 and row(8)[PUBS]==1 and tuple(row(8)[REQ:REQ+6])==(0,1,4,28,28,0),'1499ms none, 1500ms publishes {Patch,path1,target4,level5->28}'
assert leds(9)==(1<<(1+90*16//400))-1 and leds(10)==0xffff,'success sweep then all-on flash'
assert row(11)[PUBS]==1 and row(14)[AUD]==1,'continued hold no retrigger; short tap after Patch success auditions'
assert row(15)[CONSUMED]==0 and row(16)[CONSUMED]==0,'knobs not consumed in generator mode'
assert row(21)[PUBS]==3 and row(21)[ACK]==2 and leds(22) in (0xaaaa,0x5555),'refused publication shows failure pattern'
assert row(28)[LEVEL+2]==15 and row(30)[STATE]==2 and row(32)[PUBS]==3,'generator switch cancels hold'
assert row(38)[STATE]==0 and leds(38)==0 and row(39)[PUBS]==3,'PAGE exit cancels hold and releases LEDs'
assert row(48)[PUBS]==3 and row(49)[PUBS]==4 and tuple(row(49)[REQ:REQ+3])==(1,1,2),'1500ms hold across uint32 wrap on captured target'
assert row(56)[PUBS]==5 and row(56)[REQ]==4 and row(58)[CONSUMED]==1,'scale menu publishes music and consumes knobs'
assert row(61)[STATE]==0 and leds(61)==0,'scale menu PAGE exit'
assert actual==expected, 'host/ColdFire snapshots differ'
assert not bad, f'out-of-range writes: {bad[:5]}'
assert bytes(uc.mem_read(symbols['state'],16))==b'\x5a'*16,'state prefix guard damaged'
# State wrapper is immediately before output; compiler may append alignment padding.
assert bytes(uc.mem_read(symbols['state']+u32('guard_after_offset'),16))==b'\xa5'*16,'state suffix guard damaged'
report={'result':'PASS','cpu':'MCF54418 compiler / Unicorn CFV4E','events':COUNT,'snapshot_bytes':len(actual),'snapshot_sha256':hashlib.sha256(actual).hexdigest(),'publication_count':int.from_bytes(uc.mem_read(symbols['publications'],4),'big'),'instructions':instructions,'writes':writes,'max_observed_stack_bytes':sp-low_sp,'invalid_writes':bad,'elf_sha256':hashlib.sha256((D/'interface-target.elf').read_bytes()).hexdigest(),'source_sha256':{str(p.relative_to(D.parent)):hashlib.sha256(p.read_bytes()).hexdigest() for p in (D.parent/'interface/interface.c',D.parent/'interface/interface.h',D/'interface-target.c',D/'interface-target-events.inc')},'limitations':'Standalone semantic interface core. No vendor OS hooks, firmware package, real-time scheduling, instrument LEDs or hardware.'}
(D/'interface-target-report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
