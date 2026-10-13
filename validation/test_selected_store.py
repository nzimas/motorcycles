#!/usr/bin/env python3
"""Exact packaged break store (firmware/store.c, store_hooks.S), RC21:
 1. The package differs from RC19's only at the three wrapped hook sites (key,
    light, the five LED slots) and inside the three dead CHORD caves; the
    payload, cave A and the boot trampoline are byte for byte RC19's.
 2. PAGE pressed during playback surveys the current bank: patterns with
    trigs light their step key, the playing pattern flashes, other LEDs and all
    LEDs outside a break keep their mode; LED passes repaint on each flash
    phase and once when the break ends, never inside an interrupt.
 3. A step key during a break stores the variation into that slot of the bank:
    every step holds the content the live break plays there (mc_brk_step), the
    locks and ratchets are those the live message hook adds (mc_brk_msg) for
    that hit, filter targets and distortion pushes go into the stored kit; the
    slot index is stamped as the stock paste does; the pattern owner and the
    kit are notified once; the source pattern and kit and every other slot are
    untouched; press, repeats and release are consumed.
 4. The saver's 79 (slot, track) pair budget is respected; storing over the
    playing pattern works; outside a break, or with the transport stopped,
    step keys keep their stock path; garbage state RAM at boot is reset."""
import json,os,pathlib,struct
from unicorn import UC_HOOK_CODE
from unicorn import m68k_const as m
from test_native_live import LiveRig,candidate_meta,SYMBOLS
from test_native_runtime import ROOT,EVENT,STOP,SP
from validate_package import read
assert os.environ.get('MC_CANDIDATE'),'packaged candidate required'
D=pathlib.Path(__file__).resolve().parent;S=SYMBOLS;checks=[]
C=pathlib.Path(os.environ['MC_CANDIDATE'])
IMAGE=C.with_suffix('.main.bin').read_bytes()
BASE=0x40000400
RC19=D.parent/'release/R1-rc19/ModelCycles_R1-rc19_selected.local.syx'
# 1. Differences from RC19.
rc19=read(str(RC19))[2][3]
assert len(rc19)==len(IMAGE)
CAVES=[(0x400aada4,0x400aae88),(0x400a9a32,0x400a9ca8),(0x40120650,0x40121a8c)]
SITES=[0x40006100,0x40005f88]+[0x400ff9b8,0x400ffa78,0x40103070,0x401031b4,0x40107e18]
allowed=lambda a:any(lo<=a<hi for lo,hi in CAVES) or any(s<=a<s+4 for s in SITES)
diff=[BASE+i for i in range(len(IMAGE)) if IMAGE[i]!=rc19[i]]
assert diff and all(allowed(a) for a in diff),[hex(a) for a in diff if not allowed(a)][:8]
rd=lambda a,n:IMAGE[a-BASE:a-BASE+n]
assert rd(0x400060fe,6)==b'\x4e\xf9'+struct.pack('>I',S['mc_store_key_hook'])
assert rd(0x40005f86,6)==b'\x4e\xf9'+struct.pack('>I',S['mc_store_light_hook'])
for a in SITES[2:]:assert rd(a,4)==struct.pack('>I',S['mc_store_led_hook'])
for n in ('mc_store_key_hook','mc_store_light_hook','mc_store_led_hook','mc_store_key','mc_store_light','mc_store_tick'):
 assert any(lo<=S[n]<hi for lo,hi in CAVES),n
payload=C.with_suffix('.payload.bin').read_bytes()
rc19_payload=(D.parent/'build/rc19/ModelCycles-generative-selected.payload.bin')
if rc19_payload.exists():assert payload==rc19_payload.read_bytes()
checks.append('package = RC19 except the wrapped key/light/LED hooks and three dead CHORD caves; payload byte for byte RC19 (every audio routine at its RC19 address)')

STATE,BUF=0x4306f000,0x4306f100
PATS,KITS=0x41200000,0x41400000
PSTRIDE,KSTRIDE=30712,7100
STUB=0x44001000
GETTER,NOTIFY,KGET,KNOTIFY,PLAYING_OBJ,PLAYING=STUB,STUB+0x10,STUB+0x20,STUB+0x30,STUB+0x40,STUB+0x50
OVT,KVT=0x41f00000,0x41f00100
class Rig(LiveRig):
 def __init__(self,current=3,garbage=False):
  super().__init__()
  self.current=current;self.owner_notes=[];self.kit_notes=[];self.popups=[]
  for a in (0x40a78000,0x422ff000):
   try:self.u.mem_map(a,0x1000)
   except Exception:pass
  if garbage:self.u.mem_write(STATE,bytes((0x5a,0xa5)*8))
  self.w32(OVT+16,NOTIFY);self.w32(OVT+40,GETTER);self.w32(KVT+48,KGET);self.w32(KVT+92,KNOTIFY)
  self.w32(ROOT+116,KVT)
  for p in range(32):self.w32(ROOT+5192+732*p,OVT)
  for addr,fn in {GETTER:lambda a:PATS+PSTRIDE*((a[0]-ROOT-5192)//732),NOTIFY:self.onote,
                  KGET:lambda a:KITS+KSTRIDE*a[1],KNOTIFY:self.knote,
                  0x400cfd0e:lambda a:PLAYING_OBJ,0x4006a96c:lambda a:self.current,
                  S['mc_show_popup']:self.popup}.items():
   self.u.hook_add(UC_HOOK_CODE,self.stub(fn),begin=addr,end=addr)
 def onote(self,a):self.owner_notes.append(((a[0]-ROOT-5192)//732,a[1]));return 0
 def knote(self,a):assert a[0]==ROOT+116;self.kit_notes.append((a[1],a[2]));return 0
 def popup(self,a):
  t=bytes(self.u.mem_read(a[0],24));self.popups.append(t[:t.index(0)].decode());return 0
 def pattern(self,p):return bytes(self.u.mem_read(PATS+PSTRIDE*p,30710))
 def kit(self,p):return bytes(self.u.mem_read(KITS+KSTRIDE*p,7094))
 def chain(self,code,flags):
  """The patched key site: mc_store_key_hook, then RC19's mc_key_hook; the
  stock continuation (0x40006106) reports 'not consumed' as 0."""
  self.u.mem_write(EVENT,bytes(32));self.w32(EVENT+12,code);self.w32(EVENT+16,flags)
  self.w32(0xfc07000c,self.raw);self.u.mem_write(SP,struct.pack('>3I',STOP,ROOT,EVENT))
  self.u.reg_write(m.UC_M68K_REG_SR,0x2000);self.u.reg_write(m.UC_M68K_REG_A7,SP)
  self.u.emu_start(S['mc_store_key_hook'],STOP,count=3000000)
  assert self.u.reg_read(m.UC_M68K_REG_A7)==SP+4
  return self.u.reg_read(m.UC_M68K_REG_D0)
 def light(self,led,mode):
  self.w32(0xfc07000c,self.raw);return self.call('mc_store_light',led,mode)
 def active(self):return self.u.mem_read(S['mc_brk'],1)[0]
def stock_continue(u,a,n,d):
 # Stock key dispatcher after its prologue (16 bytes of saved registers): not consumed.
 sp=u.reg_read(m.UC_M68K_REG_A7)+16
 u.reg_write(m.UC_M68K_REG_D0,0);u.reg_write(m.UC_M68K_REG_PC,int.from_bytes(u.mem_read(sp,4),'big'));u.reg_write(m.UC_M68K_REG_A7,sp+4)

LENGTHS=[16,12,16,8,16,16]
def source_pattern(seed=1):
 pat=bytearray(30710)
 pat[30667]=1                                   # per-track lengths
 for t in range(6):
  o=722*t;pat[o+713:o+715]=LENGTHS[t].to_bytes(2,'big')
  for at in (128,192,256,320,384,448,516,580,644):
   for s in range(64):pat[o+at+s]=(at*7+s*13+t*29+seed)&0xff
  for s in range(64):pat[o+516+s]=255;pat[o+644+s]=255
  steps=[0,4,8,12] if t==0 else [s for s in range(LENGTHS[t]) if (s*7+t*3+seed)%5<2]
  for s in steps:pat[o+2*s:o+2*s+2]=(0x0001|(0x10 if s&1 else 0)).to_bytes(2,'big')
  L=4332+4385*t
  for s in range(64):
   for k in range(33):pat[L+68*s+2*k:L+68*s+2*k+2]=b'\xff\xff'
  # some existing locks (slot 11, and 5) and the slot-used flags
  for s in steps[:2]:
   pat[L+68*s+22:L+68*s+24]=(70<<8).to_bytes(2,'big');pat[L+68*s+10:L+68*s+12]=(33<<8).to_bytes(2,'big');pat[L+68*s+66:L+68*s+68]=(2).to_bytes(2,'big')
  if steps:pat[L+4352+11]=1;pat[L+4352+5]=1
 # track 2: a trigless lock trig and a written retrig
 o=722*2;pat[o+2*14:o+2*14+2]=(0x6801).to_bytes(2,'big');pat[o+2*15:o+2*15+2]=(0x8001).to_bytes(2,'big')
 pat[30706:30710]=struct.pack('>I',3)
 return bytes(pat)
def source_kit():
 kit=bytearray((i*5+3)&0xff for i in range(7094))
 for t in range(6):
  s=28+100*t
  for k in range(33):kit[s+20+2*k:s+22+2*k]=bytes((60,0))
  kit[s+20+20:s+22+20]=bytes((64,0))           # pitch 64
  kit[s+76:s+85]=bytes(9)                       # distortion record off
 return bytes(kit)
def setup(r,used=(3,5)):
 for p in range(16):
  r.u.mem_write(PATS+PSTRIDE*p,source_pattern() if p==3 else (source_pattern(9) if p in used else bytes(30710)))
  r.u.mem_write(KITS+KSTRIDE*p,source_kit() if p==3 else bytes((p,))*7094)
 r.u.mem_write(0x422ff168,bytes((5,2,9,1,13,7)))   # each track's step at the press
 r.hook=r.u.hook_add(UC_HOOK_CODE,stock_continue,begin=0x40006106,end=0x40006106)

# 2/4. Stopped: PAGE + step keep their stock path, no survey.
r=Rig(garbage=True);setup(r)
assert r.chain(15,1)==0 and r.active()==0
assert r.chain(16+9,1)==0 and r.chain(16+9,16)==0 and not r.owner_notes
assert r.u.mem_read(STATE,4)==b'STOR'
r.chain(15,16)
checks.append('transport stopped: PAGE and step keys keep their stock path; garbage state RAM reset to a clean state on the first key')
# Playing: PAGE starts a break and surveys bank A (patterns 3 and 5 have trigs).
r.running=1
assert r.chain(15,1)==1 and r.active()==1
used=struct.unpack('>H',bytes(r.u.mem_read(STATE+4,2)))[0];assert used==(1<<3)|(1<<5),hex(used)
modes={}
for phase in (0,1):
 r.raw=(phase<<24)|0x123
 modes[phase]=[r.light(600+i,7) for i in range(16)]
 assert r.light(999,7)==7
for i in range(16):
 if i==3:assert {modes[0][i],modes[1][i]}=={1,2}
 else:assert modes[0][i]==modes[1][i]==(2 if i==5 else 1),(i,modes)
checks.append('break: step keys show bank A, lit where a pattern has trigs, the playing pattern flashing (~4 Hz); other LEDs untouched')
# Light hook chain: mode replaced in the third argument, then RC19's mc_light_hook.
seen=[]
def stock_light(u,a,n,d):
 sp=u.reg_read(m.UC_M68K_REG_A7);seen.append(struct.unpack('>3I',bytes(u.mem_read(sp+16,12))))
 u.reg_write(m.UC_M68K_REG_PC,STOP)
h=r.u.hook_add(UC_HOOK_CODE,stock_light,begin=0x40005f8e,end=0x40005f8e)
r.raw=0;r.u.mem_write(SP,struct.pack('>4I',STOP,0x41777777,605,9));r.u.reg_write(m.UC_M68K_REG_A7,SP);r.u.reg_write(m.UC_M68K_REG_SR,0x2000);r.w32(0xfc07000c,0)
r.u.emu_start(S['mc_store_light_hook'],STOP,count=100000)
assert seen==[(0x41777777,605,2)],seen;r.u.hook_del(h)
checks.append('light hook: the store mode reaches the stock LED code through mc_light_hook (outside generator mode, mc_led_mode keeps it)')
# LED passes: repaint per phase change, never inside an interrupt.
p0=r.paints;r.raw=0;r.call('mc_store_tick');r.call('mc_store_tick');assert r.paints==p0+1
r.raw=1<<24;r.call('mc_store_tick',sr=0x2700);assert r.paints==p0+1
r.call('mc_store_tick');assert r.paints==p0+2
checks.append('LED passes repaint once per flash phase and never at interrupt level')

# 3. Store a break into slot 10 (step key 10 -> pattern A10, index 9), for a
# series of fresh breaks (each press draws a new recipe and movement layers).
totals={"trigs":0,"ratchets":0,"recipes":set(),"layers":0}
def store_and_verify(r,first):
 global ratchets,locks_checked
 plan=bytes(r.u.mem_read(S['mc_brk'],80))
 before={p:(r.pattern(p),r.kit(p)) for p in range(16)}
 assert r.chain(16+9,1)==1 and r.chain(16+9,8)==1 and r.chain(16+9,16)==1
 assert r.owner_notes==[(9,0)] and r.kit_notes==[(9,0)],(r.owner_notes,r.kit_notes)
 assert r.popups==['STORED AS A10'],r.popups
 for p in range(16):
  if p!=9:assert (r.pattern(p),r.kit(p))==before[p],p
 stored,kit=r.pattern(9),r.kit(9)
 src,src_kit=before[3]
 assert stored[30706:30710]==struct.pack('>I',9)
 used=struct.unpack('>H',bytes(r.u.mem_read(STATE+4,2)))[0];assert used==(1<<3)|(1<<5)|(1<<9)
 if first:checks.append('step key in a break: stored into that slot of the bank (A10), slot index stamped as the stock paste does, owner and kit notified once, popup, press/repeat/release consumed; source and other slots untouched')
 # Rhythm: each step holds what the live break plays there.
 SRC=0x41600000;r.u.mem_write(SRC,src)
 PER_STEP=[(0,2)]+[(at,1) for at in (128,192,256,320,384,448,516,580,644)]
 for t in range(6):
  for s in range(LENGTHS[t]):
   f=r.call('mc_brk_step',t,SRC,s);assert f<LENGTHS[t]
   o=722*t
   for at,n in PER_STEP:
    if at in (320,384,448) or at==0:continue    # flags / retrig fields: ratchets may add to them below
    assert stored[o+at+s]==src[o+at+f],(t,s,at)
   sf=int.from_bytes(src[o+2*f:o+2*f+2],'big');tf=int.from_bytes(stored[o+2*s:o+2*s+2],'big')
   assert tf&0x7fff==sf&0x7fff,(t,s,hex(sf),hex(tf))
  for s in range(LENGTHS[t],64):
   o=722*t;assert stored[o+2*s:o+2*s+2]==src[o+2*s:o+2*s+2]
 if first:checks.append('rhythm: every step of every track holds the content the live break plays on it (mc_brk_step), steps past the track length untouched')
 # Movement and ratchets: those the live message hook adds for the same hit.
 MSG,LOCK,SND=0x41630000,0x41631000,0x41632000
 ratchets=locks_checked=0
 for t in range(6):
  hit=0
  for s in range(LENGTHS[t]):
   f=r.call('mc_brk_step',t,SRC,s);o=722*t
   flags=int.from_bytes(stored[o+2*s:o+2*s+2],'big')
   if not flags&1:continue
   L=4332+4385*t
   src_words=[int.from_bytes(src[L+68*f+2*k:L+68*f+2*k+2],'big') for k in range(33)]
   entries=[(k,v) for k,v in enumerate(src_words) if v!=0xffff]
   buf=bytearray(20+8*66);buf[8:12]=struct.pack('>i',len(entries))
   for i,(k,v) in enumerate(entries):buf[20+8*i:24+8*i]=struct.pack('>HH',k,v)
   r.u.mem_write(LOCK,bytes(buf))
   sf=int.from_bytes(src[o+2*f:o+2*f+2],'big')
   msg=bytearray(80);msg[8:12]=struct.pack('>I',t);msg[40:44]=struct.pack('>I',sf&0x8000);msg[44:48]=struct.pack('>I',SND);msg[72:76]=struct.pack('>I',LOCK)
   r.u.mem_write(MSG,bytes(msg));r.u.mem_write(SND,kit[28+100*t:28+100*t+100])
   r.u.mem_write(0x40a78860+t,bytes((s,)))
   r.u.mem_write(S['mc_brk']+56+4*t,struct.pack('>I',hit));r.u.mem_write(S['mc_brk']+52,bytes(4))
   r.call('mc_brk_msg',MSG)
   n=struct.unpack('>i',bytes(r.u.mem_read(LOCK+8,4)))[0]
   live={}
   for i in range(n):k,v=struct.unpack('>HH',bytes(r.u.mem_read(LOCK+20+8*i,4)));live[k]=v
   words=[int.from_bytes(stored[L+68*s+2*k:L+68*s+2*k+2],'big') for k in range(33)]
   assert {k:v for k,v in enumerate(words) if v!=0xffff}==live,(t,s,live,words)
   assert int.from_bytes(stored[L+68*s+66:L+68*s+68],'big')==len(live)
   for k in live:assert stored[L+4352+k]==1
   locks_checked+=1
   m_=bytes(r.u.mem_read(MSG,80))
   note=not((sf&0x0800) and not (sf&0x0080))
   if sf&0x8000 or not note:
    assert stored[o+320+s]==src[o+320+f] and stored[o+384+s]==src[o+384+f] and stored[o+448+s]==src[o+448+f] and flags==sf
   elif struct.unpack('>I',m_[40:44])[0]&0x8000:
    ratchets+=1;assert flags==sf|0x8000
    assert stored[o+384+s]==struct.unpack('>i',m_[56:60])[0] and stored[o+320+s]==struct.unpack('>i',m_[60:64])[0] and stored[o+448+s]==struct.unpack('>i',m_[64:68])[0]&0xff
   else:
    assert flags==sf and stored[o+384+s]==src[o+384+f]
   hit+=1
 assert locks_checked>20
 if first:checks.append('movement: every stored trig carries exactly the pan/timbre/octave locks the live message hook adds for that hit (%d trigs), with lock counts and slot-used flags; ratchets as the live retrig (%d), trigless and written retrigs left alone'%(locks_checked,ratchets))
 # Kit: filter targets and distortion pushes; everything else as the source kit.
 p=plan;ft=[struct.unpack('b',p[32+t:33+t])[0] for t in range(6)];dp=list(p[38:44])
 exp=bytearray(src_kit)
 for t in range(6):
  d=28+100*t+76
  if ft[t]:exp[d+6]=ft[t]&0xff;exp[d+7]=exp[d+7] or 70
  if dp[t]:exp[d]=min(127,exp[d]+dp[t])
 assert kit==bytes(exp)
 if first:checks.append('kit: filter sweeps stored at their target (resonant if unset), distortion pushes added; all other kit bytes as the source kit')
 totals["trigs"]+=locks_checked;totals["ratchets"]+=ratchets;totals["recipes"].add(plan[1]);totals["layers"]|=plan[2]
 r.owner_notes.clear();r.kit_notes.clear();r.popups.clear()
store_and_verify(r,True)
for press in range(15):
 r.chain(15,16);assert r.active()==0;r.raw+=777*135168;assert r.chain(15,1)==1 and r.active()==1
 store_and_verify(r,False)
assert totals['ratchets']>0 and totals['layers']==63 and len(totals['recipes'])>=5,totals
checks.append('16 breaks stored and checked as above: %d trigs, %d ratchets, recipes %s, every movement layer'%(totals['trigs'],totals['ratchets'],sorted(totals['recipes'])))
# Further presses in the same break; release ends the break; then stock path again.
assert r.chain(16+3,1)==1 and r.chain(16+3,16)==1 and r.owner_notes[-1]==(3,0) and r.popups[-1]=='STORED AS A04'
r.chain(15,16);assert r.active()==0
p0=r.paints;r.call('mc_store_tick');assert r.paints==p0+1
assert r.light(603,7)==7
n=len(r.owner_notes);assert r.chain(16+9,1)==0 and r.chain(16+9,16)==0 and len(r.owner_notes)==n
checks.append('storing over the playing pattern works; PAGE release ends the break, LEDs repaint once back to stock, step keys take their stock path again')

# Other banks: current pattern C07 (index 38) stores into C01.
r=Rig(current=38);setup(r);r.running=1
for p in range(32,48):r.w32(ROOT+5192+732*p,OVT)
r.u.mem_write(PATS+PSTRIDE*38,source_pattern());r.u.mem_write(KITS+KSTRIDE*38,source_kit())
assert r.chain(15,1)==1
used=struct.unpack('>H',bytes(r.u.mem_read(STATE+4,2)))[0];assert used==1<<6,hex(used)
assert r.chain(16,1)==1 and r.owner_notes==[(32,0)] and r.popups==['STORED AS C01']
checks.append('banks: the survey and the store follow the playing pattern\'s bank (C07 stores into C01)')

# 4. Pair budget: a source already at 79 (slot, track) pairs gains no new slot.
r=Rig();setup(r)
pat=bytearray(source_pattern())
for t in range(6):
 for k in range(33):pat[4332+4385*t+4352+k]=0
 for st_ in range(64):pat[4332+4385*t+68*st_:4332+4385*t+68*st_+66]=b'\xff'*66;pat[4332+4385*t+68*st_+66:4332+4385*t+68*st_+68]=bytes(2)
pairs=0
for t in range(6):
 for k in range(33):
  if pairs<79 and k not in (10,11,12,13,22):pat[4332+4385*t+4352+k]=1;pairs+=1
r.u.mem_write(PATS+PSTRIDE*3,bytes(pat));r.running=1
assert r.chain(15,1)==1 and r.chain(16+9,1)==1
st=r.pattern(9)
assert sum(st[4332+4385*t+4352+k]!=0 for t in range(6) for k in range(33))==79
for t in range(6):
 for k in (10,11,12,13,22):
  if not st[4332+4385*t+4352+k]:assert all(st[4332+4385*t+68*s+2*k:4332+4385*t+68*s+2*k+2]==b'\xff\xff' for s in range(64))
checks.append('pair budget: a pattern already at the saver\'s 79 (slot, track) pairs gains no new lock slot')

report={'result':'PASS','package_sha256':candidate_meta['syx_sha256'],'checks':checks,
 'limitations':'Packaged code with synthetic pattern owners, kit container, transport, popups and repaint; the stock paste path is mirrored (two copies, owner and kit notifications), not executed; the stock saver, flash writes and PATTERN loading are not executed; LED refresh rate and flash timing not measured on hardware.'}
(D/'selected-store-report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({'result':'PASS','checks':len(checks)},indent=2))
