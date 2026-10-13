#!/usr/bin/env python3
"""Exact candidate native audition timing and actual stock note event encoding."""
import os,pathlib,json,struct,hashlib
from unicorn import UC_HOOK_CODE,UC_HOOK_MEM_WRITE
from unicorn import m68k_const as m
ROOTDIR=pathlib.Path(__file__).resolve().parents[1];C=pathlib.Path(os.environ['MC_CANDIDATE']) if os.environ.get('MC_CANDIDATE') else ROOTDIR/'build/selected-native/ModelCycles-generative-selected'
os.environ['MC_CANDIDATE']=str(C);os.environ['MC_NATIVE_PAYLOAD']=str(C.with_suffix('.payload.bin'))
from test_native_live import LiveRig,ROOT,SOUND,PATTERN,PLAN,REQUEST,checks as basechecks
checks=[]
def spy(r):
 events=[]
 for address,count,label in [(0x4008171e,7,'on'),(0x4008145e,3,'off')]:
  def hook(u,a,n,d,count=count,label=label):
   sp=u.reg_read(m.UC_M68K_REG_A7);events.append((label,list(struct.unpack('>'+'I'*count,u.mem_read(sp+4,count*4)))));u.reg_write(m.UC_M68K_REG_PC,int.from_bytes(u.mem_read(sp,4),'big'));u.reg_write(m.UC_M68K_REG_A7,sp+4)
  r.u.hook_add(UC_HOOK_CODE,hook,begin=address,end=address)
 return events

def ready(r):
 # PAGE + pad 1 (Patch), latched after PAGE release; step 16 (key 31) held 1500 ms generates.
 r.key(15,1);r.pad(1,1);r.pad(1,0);r.key(15,16);assert r.ui(0)!=0
 r.key(31,1);r.advance(1499);r.key(31,8);assert r.ui(11)==0;r.advance(1);r.key(31,8);assert r.ui(11)==1 and r.ui(57)==1;r.key(31,16)
for scenario in ['deadline','wrap','exit','running','long','slowtap','defaultnote','customnote','invalidnote']:
 r=LiveRig();events=spy(r);ready(r);old=r.capture()
 if scenario=='wrap':r.w32(r.symbols['clock_ms'],0xffffff80)
 if scenario=='running':r.running=1
 if scenario in ['defaultnote','customnote','invalidnote']:r.u.mem_write(PATTERN+722*4+712,bytes((60 if scenario=='defaultnote' else 76 if scenario=='customnote' else 200,)));old=r.capture()
 r.key(31,1)
 if scenario=='slowtap':r.advance(300);r.key(31,8);r.advance(300)  # 600 ms is no longer an audition tap
 else:r.advance(20 if scenario!='long' else 1500)
 if scenario=='long':r.key(31,8)
 r.key(31,16)
 if scenario in ['running','long','slowtap','invalidnote']:assert not events
 else:
  note=60 if scenario=='defaultnote' else 76 if scenario=='customnote' else 64
  assert events==[('on',[4,note,100,64,0,0xffffffff,0xffffffff])]
  assert r.capture()==old,'audition changed canonical pattern/sounds'
  if scenario=='customnote':r.u.mem_write(PATTERN+722*4+712,b'\x18') # off must retain auditioned76 despite later tracknote change
  if scenario=='exit':r.key(15,1);r.key(15,16);assert events[-1]==('off',[4,note,64]) and r.ui(0)==0
  else:
   r.advance(149);r.call('mc_clock');assert len(events)==1;r.advance(1);r.call('mc_clock');assert events[-1]==('off',[4,note,64]);r.advance(1000);r.call('mc_clock');assert len(events)==2
 checks.append('native shorttap/'+scenario+' verified note ABI and release')
# Execute selected package note primitives themselves, observing only final
# audio/UI queue boundaries and synthetic position metadata (not replacing note-on).
r=LiveRig();audio=[];ui=[];writes=[];bank=0x406fa040
r.w32(0x40a7887c,bank);r.w32(0x40a700c4,0xffffffff);r.u.mem_write(bank+722*4+710,b'\x06\x80');r.u.mem_write(0x40fb680c,b'\xff'*(4*128*6))
def posted(args):audio.append(bytes(r.u.mem_read(args[0],56)));return 0
def position(args):r.u.mem_write(args[2],b'\x00\x00');return 0
def ui_post(args):ui.append(bytes(r.u.mem_read(args[0],32)));return 0
for address,fn in [(0x4005894a,posted),(0x40056178,position),(0x40080bf6,ui_post)]:r.u.hook_add(UC_HOOK_CODE,r.stub(fn),begin=address,end=address)
r.symbols=dict(r.symbols,note_on=0x4008171e,note_off=0x4008145e)
r.u.hook_add(UC_HOOK_MEM_WRITE,lambda u,t,a,n,v,d:writes.append((a,n)))
before=bytes(r.u.mem_read(bank,30710));r.call('note_on',4,60,100,64,0,0xffffffff,0xffffffff);assert len(audio)==1
on=audio[0];assert struct.unpack('>4I',on[:16])== (4,60,100,1);assert int.from_bytes(on[20:24],'big')==0xffffffff and int.from_bytes(on[32:36],'big')==64
r.call('note_off',4,60,64);assert len(audio)==2;off=audio[1];assert int.from_bytes(off[:4],'big')==4 and int.from_bytes(off[4:8],'big')==60
assert bytes(r.u.mem_read(bank,30710))==before;assert not any(bank<=a<bank+30710 for a,n in writes)
checks.append('actual selected note-on/off primitives encode target4 note60 and preserve canonical track/locks')
manifest=json.loads(C.with_suffix('.manifest.json').read_text());assert manifest['sources']['firmware/runtime.c']==hashlib.sha256((ROOTDIR/'firmware/runtime.c').read_bytes()).hexdigest()
report={'result':'PASS','package_sha256':hashlib.sha256(C.with_suffix('.syx').read_bytes()).hexdigest(),'baseline_checks':len(basechecks),'checks':checks,'actual_note_on_event_hex':on.hex(),'actual_note_off_event_hex':off.hex(),'ui_queue_events':len(ui),'limitations':'Native timing tests spy note boundaries. Separate primitive proof executes actual selected note-on/off, including Model-TG entry/scale hooks, with final audio/UI queue and position metadata stubs. Synthetic initialized OS bindings; no queue consumption, DSP render, RTOS/peripheral timing or hardware audition proof.'}
(ROOTDIR/'validation/patch-audition-native-report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
