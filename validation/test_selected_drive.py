#!/usr/bin/env python3
"""Exact packaged distortion stage:
 1. Audio: the patched call at 0x40059852 runs the stock Volume+Dist stage,
    then the per-track stage on the six 32-sample Q31 blocks, reading each
    sound's record at +76..+83. Bypass tracks are untouched; active tracks
    match the host C reference (firmware/drive.c) word for word; cost measured.
 2. Persistence: the wrapped stock savers/loader carry +76..+83 through the
    serialized record and otherwise behave exactly like the stock routines.
 3. FUNC + SWEEP: turning SWEEP with FUNC held changes the selected track's
    amount (clamped 0..127) without touching anything else, and the sound
    owner is notified once turning stops."""
import ctypes,json,os,pathlib,random,struct,subprocess,tempfile
from unicorn import Uc,UC_ARCH_M68K,UC_MODE_BIG_ENDIAN,UC_HOOK_CODE
from unicorn import m68k_const as m
from test_native_live import LiveRig,SOUND,ROOT,candidate_meta
assert os.environ.get('MC_CANDIDATE'),'packaged candidate required'
D=pathlib.Path(__file__).resolve().parent;PROJECT=D.parent
checks=[];rng=random.Random(12)
# Host reference of the same kernel.
lib_path=pathlib.Path(tempfile.mkdtemp())/'drive.dylib'
subprocess.run(['cc','-std=c11','-O1','-shared',str(PROJECT/'firmware/drive.c'),str(PROJECT/'firmware/djfilter.c'),'-o',str(lib_path)],check=True)
ref=ctypes.CDLL(str(lib_path))
def reference(block,params):
 # Distortion (bytes 6/7 are not its settings), then the DJ filter.
 state=(ctypes.c_uint8*1024)();buf=(ctypes.c_int32*32)(*block);p=(ctypes.c_uint8*8)(*(list(params[:6])+[0,0]))
 if params[0]:ref.mc_drive_block(buf,p,state)
 fstate=(ctypes.c_uint8*64)();ref.mc_djfilter_block(buf,ctypes.c_int8(ctypes.c_int8(params[6]).value),ctypes.c_uint8(params[7]),fstate)
 return list(buf)

# ---- 1. Audio hook --------------------------------------------------------
BLOCKS,PARAMS,KIT=0x80001858,0x8000101a-14,0x41200000
heavy=[[127,127,127,127,127,127,0,0]]*6
def trimmed(records):
 cost=lambda d:0 if not d[0] else 21+(9 if d[4] else 0)+(9 if d[3] else 0)+(24 if d[2] else 18 if d[5] else 0)
 p=[list(x) for x in records];total=sum(cost(d)+(37 if d[6] else 0) for d in p)
 while total>240 and any(cost(d) for d in p):
  w=max(range(6),key=lambda t:(cost(p[t]),-t));d=p[w];before=cost(d)
  if d[2]:d[2]=0
  elif d[3]:d[3]=0
  elif d[5]:d[5]=0
  elif d[4] or d[1]:d[1]=0;d[4]=0
  else:d[0]=0
  total-=before-cost(d)
 return p
LOCKED=0x41210000
def audio_case(records,label,owner=None,locked=()):
 """owner: the UI owner records (what FUNC+SWEEP/PITCH edit), mirrored by the
 UI clock; the kit the audio engine points at then holds stale bytes. locked:
 tracks whose sound is replaced by a sound lock carrying `records`."""
 r=LiveRig()
 kit=bytearray(28+600)
 for t in range(6):
  base=28+100*t;kit[base+38]=t%6;kit[base+76:base+84]=bytes(records[t]) if owner is None else bytes((t*37+i*11)&127 for i in range(8))
 r.u.mem_write(KIT,bytes(kit));r.w32(0x800017e4,KIT)
 for t in range(6):r.w32(0x800017ec+4*t,0)
 if owner is not None:
  for t in range(6):r.u.mem_write(SOUND+100*t+76,bytes(owner[t]))
  r.call('mc_clock')
  for t in range(6):r.u.mem_write(SOUND+100*t+76,bytes(8))   # mirror, not a live read
  for t in range(6):r.w32(0x800017ec+4*t,KIT+28+100*t)
  for t in locked:
   r.u.mem_write(LOCKED+100*t+76,bytes(records[t]));r.w32(0x800017ec+4*t,LOCKED+100*t)
  records=[records[t] if t in locked else owner[t] for t in range(6)]
 # Stock Volume+Dist reads k19 per track: 60 is a bit-exact passthrough.
 params=bytearray(14+66*6)
 for t in range(6):params[14+66*t+38:14+66*t+40]=struct.pack('>H',60<<8)
 r.u.mem_write(PARAMS,bytes(params))
 blocks=[[rng.randrange(-2**30,2**30) for _ in range(32)] for _ in range(6)]
 r.u.mem_write(BLOCKS,b''.join(struct.pack('>32i',*b) for b in blocks))
 sp=0x4400f000;r.u.mem_write(sp,struct.pack('>II',BLOCKS,PARAMS))
 count=[0];h=r.u.hook_add(UC_HOOK_CODE,lambda u,a,n,d:count.__setitem__(0,count[0]+1))
 regs=[m.UC_M68K_REG_D2,m.UC_M68K_REG_D3,m.UC_M68K_REG_D4,m.UC_M68K_REG_D5,m.UC_M68K_REG_D6,m.UC_M68K_REG_D7,m.UC_M68K_REG_A2,m.UC_M68K_REG_A3,m.UC_M68K_REG_A4,m.UC_M68K_REG_A5,m.UC_M68K_REG_A6]
 for i,reg in enumerate(regs):r.u.reg_write(reg,0x5a5a0000+i)
 r.u.reg_write(m.UC_M68K_REG_SR,0x2700);r.u.reg_write(m.UC_M68K_REG_A7,sp)
 # The stock stage uses the EMAC unit, which Unicorn does not emulate; stub it
 # and record its arguments (at Volume+Dist 60 it is a bit-exact passthrough).
 stock_args=[]
 r.u.hook_add(UC_HOOK_CODE,r.stub(lambda a:stock_args.append(a[:2]) or 0),begin=0x40056f58,end=0x40056f58)
 r.u.emu_start(0x40059852,0x40059858,count=2_000_000)
 assert stock_args==[(BLOCKS,PARAMS)],stock_args
 assert r.u.reg_read(m.UC_M68K_REG_PC)==0x40059858 and r.u.reg_read(m.UC_M68K_REG_A7)==sp
 assert [r.u.reg_read(reg) for reg in regs]==[0x5a5a0000+i for i in range(len(regs))],'callee-saved registers'
 out=[list(struct.unpack('>32i',bytes(r.u.mem_read(BLOCKS+128*t,128)))) for t in range(6)]
 for t in range(6):
  eff=trimmed(records)[t]
  expect=blocks[t] if eff[0]==0 and eff[6]==0 else reference(blocks[t],eff)
  assert out[t]==expect,(label,t)
 first=count[0]
 # Steady state: keep feeding blocks until the parameter glide has settled.
 for _ in range(200):  # glide lasts 60..100 ms (~90..150 blocks)
  r.u.mem_write(BLOCKS,b''.join(struct.pack('>32i',*[rng.randrange(-2**30,2**30) for _ in range(32)]) for _ in range(6)))
  r.u.mem_write(sp,struct.pack('>II',BLOCKS,PARAMS));r.u.reg_write(m.UC_M68K_REG_SR,0x2700);r.u.reg_write(m.UC_M68K_REG_A7,sp);count[0]=0
  r.u.emu_start(0x40059852,0x40059858,count=2_000_000)
 r.u.hook_del(h);return first,count[0]
base=audio_case([[0]*8]*6,'all bypass')
plain=audio_case([[90,0,0,0,0,0,0,0]]*6,'six plain')
budget=audio_case([[100,60,0,40,50,0,0,0],[80,30,0,0,0,90,0,0],[70,0,60,0,0,0,0,0],[127,127,0,127,127,0,0,0],[40,0,0,0,30,0,0,0],[0,0,0,0,0,0,0,0]],'mixed kit')
audio_case([[90,40,0,30,20,0,226,80],[0,0,0,0,0,0,40,127],[60,0,50,0,0,0,193,0],[0,0,0,0,0,0,0,0],[127,127,0,0,127,0,63,20],[0,0,0,0,0,0,200,60]],'DJ filter after the gated stage')
checks.append('DJ filter applied after the gated distortion on every track, also with the distortion off')
SET=[[90,40,0,30,20,0,226,80],[0,0,0,0,0,0,40,127],[60,0,50,0,0,0,193,0],[0,0,0,0,0,0,0,0],[127,127,0,0,127,0,63,20],[0,0,0,0,0,0,200,60]]
audio_case([[0]*8]*6,'UI owner records mirrored by the clock',owner=SET)
audio_case([[0,0,0,0,0,0,30,90]]*6,'sound lock overrides the mirror',owner=SET,locked=(2,5))
checks.append('audio uses the UI owner records (mirrored by the UI clock), not stale kit bytes; a sound lock uses its own sound')
worst=audio_case(heavy,'six tracks everything on (CPU guard)')
checks.append('CPU guard trims an over-budget kit exactly as specified')
checks.append('patched 0x40059852 calls stock Volume+Dist with the caller arguments, then the stage; bypass tracks bit-exact, active tracks match host reference, callee registers preserved')
cost={'bypass_all_block':base[1],'six_plain_first_block':plain[0],'six_plain_steady_block':plain[1],'mixed_kit_first_block':budget[0],'mixed_kit_steady_block':budget[1],
 'steady_per_sample_track_plain':round((plain[1]-base[1])/(6*32),1),'steady_per_sample_track_mixed_kit':round((budget[1]-base[1])/(5*32),1),'guarded_all_on_steady_block':worst[1]}

# ---- 1b. Noise gate ----------------------------------------------------------
def gate_run(sequence,record):
 """Feed track 1 a list of block amplitudes; return output blocks and per-block cost."""
 r=LiveRig();kit=bytearray(28+600);kit[28+76:28+84]=bytes(record)
 r.u.mem_write(KIT,bytes(kit));r.w32(0x800017e4,KIT)
 for t in range(6):r.w32(0x800017ec+4*t,0)
 params=bytearray(14+66*6)
 for t in range(6):params[14+66*t+38:14+66*t+40]=struct.pack('>H',60<<8)
 r.u.mem_write(PARAMS,bytes(params))
 r.u.hook_add(UC_HOOK_CODE,r.stub(lambda a:0),begin=0x40056f58,end=0x40056f58)
 count=[0];r.u.hook_add(UC_HOOK_CODE,lambda u,a,n,d:count.__setitem__(0,count[0]+1))
 sp=0x4400f000;outs=[];ins=[];costs=[]
 for amp in sequence:
  block=[int(amp*((-1)**i)*(0.5+0.5*((i*7)%5)/4)) for i in range(32)]
  ins.append(block)
  r.u.mem_write(BLOCKS,struct.pack('>32i',*block)+bytes(128*5))
  r.u.mem_write(sp,struct.pack('>II',BLOCKS,PARAMS));r.u.reg_write(m.UC_M68K_REG_SR,0x2700);r.u.reg_write(m.UC_M68K_REG_A7,sp);count[0]=0
  r.u.emu_start(0x40059852,0x40059858,count=2_000_000)
  outs.append(list(struct.unpack('>32i',bytes(r.u.mem_read(BLOCKS,128)))));costs.append(count[0])
 return ins,outs,costs
REC=[110,80,60,40,60,0,0,0]
quiet=2**31*10**(-72/20)          # -72 dBFS: below the gate
loud=2**31*10**(-12/20)
ins,outs,costs=gate_run([quiet]*20,REC)
assert all(o==i for o,i in zip(outs,ins)),'quiet input passes untouched through a closed gate'
closed_cost=costs[-1]
ins,outs,costs=gate_run([loud]*10+[quiet]*120,REC)
assert outs[0]!=ins[0],'gate opens on the first loud block'
assert all(outs[k]!=ins[k] for k in range(10,10+30)),'hold keeps the stage on ~20 ms'
assert all(outs[k]==ins[k] for k in range(10+15+30+61,130)),'after envelope fall, hold and 40 ms release the clean signal passes untouched'
checks.append('noise gate: quiet input untouched, instant open on a hit, 20 ms hold, 40 ms release to the clean signal')
cost['gate_closed_block']=closed_cost

# ---- 2. Persistence -------------------------------------------------------
stock=Uc(UC_ARCH_M68K,UC_MODE_BIG_ENDIAN);stock.ctl_set_cpu_model(m.UC_CPU_M68K_CFV4E)
stock.mem_map(0x40000000,0x2000000);stock.mem_map(0x44000000,0x100000)
stock.mem_write(0x40000400,(PROJECT/'build/notg-main.bin').read_bytes())
def scall(u,addr,*args):
 u.mem_write(0x4400ff00,struct.pack('>I'+'I'*len(args),0x44000000,*args));u.reg_write(m.UC_M68K_REG_SR,0x2700);u.reg_write(m.UC_M68K_REG_A7,0x4400ff00)
 u.emu_start(addr,0x44000000,count=1_000_000);return u.reg_read(m.UC_M68K_REG_D0)
RUN,STO,RUN2=0x41300000,0x41300200,0x41300400
for case in range(12):
 r=LiveRig()
 runtime=bytearray(100);runtime[4:20]=b'DRIVE TEST\0\0\0\0\0\0'
 for k in range(23):runtime[20+2*k]=rng.randrange(0,128)
 runtime[38]=rng.randrange(0,7);runtime[40]=rng.randrange(40,89)
 record=bytes([rng.randrange(1,128),rng.randrange(0,128),rng.randrange(0,128),rng.randrange(0,128),rng.randrange(0,128),rng.randrange(0,128),0,0])
 runtime[76:84]=record
 for addr in (0x4005afa0,0x4005b054):
  for u,call in ((stock,lambda *a:scall(stock,*a)),(r.u,lambda *a:scall(r.u,*a))):
   u.mem_write(RUN,bytes(runtime));u.mem_write(STO,bytes(100))
  scall(stock,addr,STO,RUN);scall(r.u,addr,STO,RUN)
  s_rec=bytes(stock.mem_read(STO,100));p_rec=bytes(r.u.mem_read(STO,100))
  if addr==0x4005afa0:v2_rec=s_rec
  total=sum(record)&255
  assert p_rec[76:84]==record and p_rec[88:92]==bytes((0x44,0x31,total,total^0xa5)),(case,hex(addr))
  assert p_rec[:76]+p_rec[84:88]+p_rec[92:]==s_rec[:76]+s_rec[84:88]+s_rec[92:],(case,hex(addr))
  for u in (stock,r.u):u.mem_write(STO,p_rec);u.mem_write(RUN2,b'\xa5'*100)
  rs=scall(stock,0x4005aecc,RUN2,STO);rp=scall(r.u,0x4005aecc,RUN2,STO)
  s_run=bytes(stock.mem_read(RUN2,100));p_run=bytes(r.u.mem_read(RUN2,100))
  # The stock loader accepts version-2 records only; v1 is refused by both.
  assert (rs&255)==(rp&255)==(1 if addr==0x4005afa0 else 0),(case,hex(addr),rs,rp)
  if rs&255:assert p_run[76:84]==record and p_run[:76]+p_run[84:]==s_run[:76]+s_run[84:],(case,hex(addr))
  else:assert p_run==s_run,(case,hex(addr))
checks.append('wrapped v1/v2 savers and loader round-trip +76..+83 with a signature at +88..+91; every other byte equals the stock routines')
# Model-TG-era record (RC1..RC9): its own data at +76..+87, no signature -> stage off.
for case in range(6):
 r=LiveRig();tg=bytearray(v2_rec);tg[76:88]=bytes(rng.randrange(1,256) for _ in range(12));tg[88:92]=bytes(4)
 r.u.mem_write(STO,bytes(tg));r.u.mem_write(RUN2,b'\xa5'*100);assert scall(r.u,0x4005aecc,RUN2,STO)&255==1
 assert bytes(r.u.mem_read(RUN2+76,8))==bytes(8),case
 bad=bytearray(p_rec if False else v2_rec);bad[76:84]=bytes(range(1,9));s=sum(bad[76:84])&255;bad[88:92]=bytes((0x44,0x31,s,(s^0xa5)^1))
 r.u.mem_write(STO,bytes(bad));r.u.mem_write(RUN2,b'\xa5'*100);scall(r.u,0x4005aecc,RUN2,STO);assert bytes(r.u.mem_read(RUN2+76,8))==bytes(8),case
checks.append('Model-TG-era and corrupted records load with the stage off')
# A stock record (zeros at +76..+83) loads with the stage off.
r=LiveRig();r.u.mem_write(STO,bytes(v2_rec[:76])+bytes(8)+bytes(v2_rec[84:]));r.u.mem_write(RUN2,b'\xa5'*100);scall(r.u,0x4005aecc,RUN2,STO)
assert bytes(r.u.mem_read(RUN2+76,8))==bytes(8);checks.append('stock records load with the stage off')

# ---- 3. FUNC + SWEEP --------------------------------------------------------
r=LiveRig();func=[1]
r.u.hook_add(UC_HOOK_CODE,r.stub(lambda a:func[0]),begin=0x4007faf4,end=0x4007faf4)
for track in (0,3,5):
 r.selected=track;addr=SOUND+100*track;r.u.mem_write(addr+76,bytes((20,1,2,3,4,5,0,0)));before=bytes(r.u.mem_read(SOUND,600))
 assert r.enc(6,9)==1;assert r.u.mem_read(addr+76,1)[0]==29
 assert r.enc(6,200)==1;assert r.u.mem_read(addr+76,1)[0]==127
 assert r.enc(6,-500)==1;assert r.u.mem_read(addr+76,1)[0]==0
 after=bytes(r.u.mem_read(SOUND,600));assert after[:100*track+76]+after[100*track+77:]==before[:100*track+76]+before[100*track+77:]
 caches=r.caches()[track];r.advance(299);r.call('mc_clock');assert r.caches()[track]==caches,'notify waits for turning to stop'
 r.advance(2);r.call('mc_clock');assert r.caches()[track]==0,'owner notified after 300 ms'
 r.w32(0x800015a0+(147+track)*4,0x12340000+track)
# FUNC + PITCH: DJ filter position, signed byte 6, clamped -63..63.
func[0]=1
for track in (1,4):
 r.selected=track;addr=SOUND+100*track;r.u.mem_write(addr+76,bytes((20,1,2,3,4,5,0,9)));before=bytes(r.u.mem_read(SOUND,600))
 assert r.enc(2,-10)==1;assert r.u.mem_read(addr+82,1)[0]==(256-10)
 assert r.enc(2,-100)==1;assert r.u.mem_read(addr+82,1)[0]==(256-63)
 assert r.enc(2,200)==1;assert r.u.mem_read(addr+82,1)[0]==63
 assert r.enc(2,-63)==1;assert r.u.mem_read(addr+82,1)[0]==0
 after=bytes(r.u.mem_read(SOUND,600));assert after[:100*track+82]+after[100*track+83:]==before[:100*track+82]+before[100*track+83:]
checks.append('FUNC+PITCH sets the selected track DJ filter (signed, clamped -63..63), changes nothing else')
func[0]=0;r.selected=2;assert r.enc(6,5)==0,'SWEEP without FUNC goes to the stock path';assert r.enc(2,5)==0,'PITCH without FUNC goes to the stock path'
checks.append('FUNC+SWEEP sets the selected track amount, clamps 0..127, changes nothing else, notifies after 300 ms; plain SWEEP untouched')

report={'result':'PASS','package_sha256':candidate_meta['syx_sha256'],'checks':checks,'emulated_instructions':cost,
 'limitations':'Synthetic kit/params/SRAM blocks; overlay drawing not rendered (popup machinery absent in the rig); instruction counts, not cycles.'}
(D/'selected-drive-report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({'result':'PASS','checks':len(checks),'cost':cost},indent=2))
