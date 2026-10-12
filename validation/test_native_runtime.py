#!/usr/bin/env python3
"""Actual compiled native runtime with explicit synthetic OS object bindings."""
import json,pathlib,struct,hashlib,os
from unicorn import Uc,UC_ARCH_M68K,UC_MODE_BIG_ENDIAN,UC_HOOK_CODE
from unicorn import m68k_const as m
D=pathlib.Path(__file__).resolve().parent
SYM={l.split()[2]:int(l.split()[0],16) for l in (D/'native-runtime-symbols.txt').read_text().splitlines()}
ROOT,KIT,VIEW,EVENT,MAP,VTABLE,SOUND=0x41010000,0x41020000,0x41030000,0x41040000,0x41050000,0x41060000,0x41100000
STOP,SP=0x45000000,0x4400ffe0
class Rig:
 def __init__(self,raw=0,binary=None,symbols=None,main_image=None):
  self.symbols=symbols or SYM
  self.u=Uc(UC_ARCH_M68K,UC_MODE_BIG_ENDIAN);self.u.ctl_set_cpu_model(m.UC_CPU_M68K_CFV4E)
  # 0x43000000..0x43100000 holds MACRO and, on the selected build, the native runtime.
  for base,size in [(0x40000000,0x2000000),(0x43000000,0x100000),(0x43100000,0x20000),(0x44000000,0x10000),(STOP,0x1000),(0x46700000,0x100000),(0xfc070000,0x1000)]:self.u.mem_map(base,size)
  payload_at=0x43000000
  if os.environ.get('MC_NATIVE_PAYLOAD'):
   manifest=pathlib.Path(os.environ['MC_NATIVE_PAYLOAD']).with_name(pathlib.Path(os.environ['MC_NATIVE_PAYLOAD']).name.replace('.payload.bin','.manifest.json'))
   payload_at=json.loads(manifest.read_text())['payload_start']
  self.u.mem_write(payload_at,(pathlib.Path(os.environ['MC_NATIVE_PAYLOAD']) if os.environ.get('MC_NATIVE_PAYLOAD') else pathlib.Path(binary) if binary else D/'native-runtime.bin').read_bytes());self.raw=raw;self.selected=4;self.delta=1;self.paints=0;self.min_sp=SP
  if main_image is not None:
   self.u.mem_write(0x40000400,main_image)
   if os.environ.get('MC_NATIVE_PAYLOAD'):
    self.u.reg_write(m.UC_M68K_REG_A7,SP)
    # Standalone MACRO's copier fills 0x43000000.. then resumes the stock BSS
    # clear at 0x400004ba; stop there so the rig's own memory survives.
    self.u.emu_start(0x4016cae8,0x400004ba,count=1000000)
    assert self.u.reg_read(m.UC_M68K_REG_PC)==0x400004ba
  for i in range(6):
   self.w32(ROOT+212+68*i,VTABLE);self.u.mem_write(SOUND+100*i+38,bytes((i,0)))
  self.w32(VTABLE+40,0x40015000)
  self.w32(0x404a8bdc,MAP)
  for i in range(16):self.w32(MAP+4*i,600+i)
  bindings={0x400cf866:lambda args:ROOT,0x4000eb90:lambda args:KIT,0x40012412:lambda args:self.selected,0x400d0974:lambda args:ROOT,0x400060d8:lambda args:VIEW,0x40076c04:self.paint,0x4006f73a:lambda args:self.delta,0x40015000:self.data}
  for addr,fn in bindings.items():self.u.hook_add(UC_HOOK_CODE,self.stub(fn),begin=addr,end=addr)
  self.u.hook_add(UC_HOOK_CODE,self.stack)
  self.call('mc_native_init')
 def stack(self,u,a,n,d):self.min_sp=min(self.min_sp,u.reg_read(m.UC_M68K_REG_A7))
 def w32(self,a,v):self.u.mem_write(a,struct.pack('>I',v&0xffffffff))
 def value(self,name):return int.from_bytes(self.u.mem_read(self.symbols[name],4),'big')
 def ui(self,n):return self.u.mem_read(self.symbols['mc_native']+n,1)[0]
 def data(self,args):
  assert (args[0]-ROOT-212)%68==0
  i=(args[0]-ROOT-212)//68;assert 0<=i<6;return SOUND+i*100
 def paint(self,args):assert args[0]==VIEW;self.paints+=1;return 0
 def stub(self,fn):
  def hook(u,a,n,d):
   sp=u.reg_read(m.UC_M68K_REG_A7);args=struct.unpack('>4I',u.mem_read(sp+4,16));u.reg_write(m.UC_M68K_REG_D0,fn(args)&0xffffffff);u.reg_write(m.UC_M68K_REG_PC,int.from_bytes(u.mem_read(sp,4),'big'));u.reg_write(m.UC_M68K_REG_A7,sp+4)
  return hook
 def call(self,name,*args,sr=0x2000):
  self.w32(0xfc07000c,self.raw);self.u.mem_write(SP,struct.pack('>I'+'I'*len(args),STOP,*args));self.u.reg_write(m.UC_M68K_REG_SR,sr);self.u.reg_write(m.UC_M68K_REG_A7,SP);self.u.emu_start(self.symbols[name],STOP,count=1000000);assert self.u.reg_read(m.UC_M68K_REG_PC)==STOP;assert self.u.reg_read(m.UC_M68K_REG_A7)==SP+4;return self.u.reg_read(m.UC_M68K_REG_D0)
 def advance(self,ms):self.raw=(self.raw+ms*135168)&0xffffffff
 def key(self,code,flags):self.u.mem_write(EVENT,bytes(32));self.w32(EVENT+12,code);self.w32(EVENT+16,flags);return self.call('mc_key',ROOT,EVENT)
 def pad(self,index,down):self.u.mem_write(EVENT,bytes(32));self.w32(EVENT+20,index);self.w32(EVENT+16,down);return self.call('mc_pad',ROOT,EVENT)
 def enc(self,index,delta):self.delta=delta;self.u.mem_write(EVENT,bytes(32));self.w32(EVENT+12,index);return self.call('mc_encoder',ROOT,EVENT)
def level_amount(level):return 0 if level<1 else 100 if level>15 else (level-1)*100//14
class Publications:
 """Commit observer: the standalone build's stub counters, or, for the packaged
 candidate, a spy on the real mc_commit_live entry (request/plan arguments)."""
 def __init__(self,r,packaged):
  self.r,self.packaged,self.seen=r,packaged,[]
  if packaged:r.u.hook_add(UC_HOOK_CODE,self.entry,begin=r.symbols['mc_commit_live'],end=r.symbols['mc_commit_live'])
 def entry(self,u,a,n,d):
  sp=u.reg_read(m.UC_M68K_REG_A7);ctx,plan,request=struct.unpack('>3I',u.mem_read(sp+4,12))
  masks=bytes(u.mem_read(plan+60+6*1282,2))  # mc_plan.sound_mask/sequence_mask (mc_phrase is 1282 bytes)
  self.seen.append(dict(request=bytes(u.mem_read(request,12)),sound_mask=masks[0],sequence_mask=masks[1]))
 def count(self):return len(self.seen) if self.packaged else self.r.value('publications')
 def last(self,field):
  if not self.packaged:return self.r.value({'target':'last_target','seed':'last_seed','generator':'last_generator','path':'last_path','sound_mask':'last_sound_mask','sequence_mask':'last_sequence_mask'}[field])
  q=self.seen[-1]
  if field in ('sound_mask','sequence_mask'):return q[field]
  return {'generator':q['request'][0],'path':q['request'][1],'target':q['request'][2],'density':q['request'][3],'brutality':q['request'][4],'seed':int.from_bytes(q['request'][8:12],'big')}[field]
def main():
 packaged=bool(os.environ.get('MC_CANDIDATE'))
 if packaged:
  # Exact packaged MAIN + payload with the live-rig bindings (importing runs that suite too).
  from test_native_live import LiveRig as R,candidate_meta
 else:R=Rig
 checks=[]
 def check(yes,name):assert yes,name;checks.append(name)
 def leds(r):return [r.call('mc_led_mode',600+i,7) for i in range(16)]
 r=R(0xffffffff-100000);r.raw=(r.raw+135167)&0xffffffff;check(r.call('mc_now')==0,'fraction before1ms across rawDMAwrap');r.raw=(r.raw+1)&0xffffffff;check(r.call('mc_now')==1,'fraction accumulation and rawDMAwrap');r.advance(1500);check(r.call('mc_now')==1501,'exact1500ms clock delta')
 r=R(0xffffffff-50000000);p=Publications(r,packaged)
 check(r.key(15,1)==0,'ordinary PAGEpress forwarded');check(r.pad(1,1)==1,'generator pad press consumed');r.pad(1,0);check(r.ui(4)==4 and r.ui(0)==2,'target captured before pad ordinary action, READY');r.selected=0
 check(leds(r)[:15]==[2]*7+[1]*8,'default level7 bar on steps1..7')
 check(r.key(19,1)==1 and r.key(19,16)==1 and r.ui(62)==4 and leds(r)[:15]==[2]*4+[1]*11,'step4 press/release consumed, sets Patch level4 bar')
 r.key(31,1);r.advance(1499);r.key(31,8);check(p.count()==0,'1499ms repeated step16 no generation');r.advance(1);r.key(31,8)
 check(p.count()==1 and p.last('target')==4 and p.last('sound_mask')==16 and r.ui(11)==1,'1500ms step16 repeat generates captured track across rawDMAwrap')
 if packaged:check(p.last('generator')==0 and p.last('path')==1 and p.last('density')==p.last('brutality')==level_amount(4)==21,'request path1, density=brutality=level4 amount')
 seed=p.last('seed');r.advance(1000);r.key(31,8);check(p.count()==1,'continued native repeats exactlyonce');r.key(31,16);r.key(31,1);r.advance(1500);r.key(31,8);check(p.count()==2 and p.last('seed')!=seed,'fresh native hold freshseed')
 check(r.enc(2,100)==0 and r.enc(3,-100)==0 and r.ui(62)==4,'Density/Brutality knobs forwarded (ordinary function) in generator mode');check(r.enc(4,100)==0,'other physical knob forwarded')
 r.key(31,16);r.key(15,16);check(r.ui(0)!=0,'PAGE release keeps generator latched')
 r.key(31,1);r.key(20,1);r.advance(500);r.key(15,1);r.key(15,16);r.advance(1200);r.key(31,8);check(p.count()==2 and r.ui(0)==0 and r.call('mc_led_mode',600,7)==7,'PAGEcancel restores stockLEDmode')
 check(r.key(20,16)==1,'owned level keyrelease consumed after PAGE');check(r.key(20,16)==0,'duplicate level release forwarded')
 r.key(31,16)
 r.key(15,1);r.pad(4,1);r.pad(4,0);check(r.ui(2)==3 and r.ui(3)==1 and r.ui(4)==0,'Pattern entry captures selected track0');r.key(31,1);r.advance(1500);before=r.paints;r.call('mc_clock',sr=0x2600);check(p.count()==2 and r.paints==before,'elevatedIPL LEDclock does no publication or repaint');r.call('mc_clock');check(p.count()==2,'IPL0 LEDclock onlyfeedback neverpublishes');r.key(31,8);check(p.count()==3 and p.last('sequence_mask')==63,'Pattern native keyrepeat publishesallsequence')
 r.u.mem_write(SOUND+38,bytes((0,1)));r.key(31,16);r.key(31,1);r.advance(1500);r.key(31,8);check(p.count()==3 and r.ui(11)==2,'fractional canonical machine failclosed')
 elf=pathlib.Path(os.environ['MC_CANDIDATE']).with_suffix('.elf') if packaged else D/'native-runtime.elf'
 report={'result':'PASS','checks':checks,'max_observed_stack_bytes':SP-r.min_sp,'elf_sha256':hashlib.sha256(elf.read_bytes()).hexdigest(),'limitations':'Actual compiled runtime/interface/generation calls with synthetic OS accessors, encoder delta and publish callback. Not exact firmware package or real OS publication/dispatch.'}
 if packaged:report.update(package_sha256=candidate_meta['syx_sha256'],limitations='Exact packaged MAIN and payload (live-rig synthetic owner trees, transport and heap); publications observed by a spy on the real mc_commit_live entry. Not complete boot, real OS dispatch or hardware.')
 (D/('native-runtime-packaged-report.json' if packaged else 'native-runtime-report.json')).write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))

if __name__=="__main__":main()
