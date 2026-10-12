#!/usr/bin/env python3
"""Exact packaged LYRA (machine 5, replaces CHORD; firmware/lyra.c):
 1. Patched image: voice-loop update/render entry 5 -> lyra_update/lyra_render,
    name "Lyra", descriptors 71..74 renamed with new defaults and SHAPE 0..127,
    SHAPE knob graphic/detents as TONE, old-file CHORD conversion disabled.
 2. lyra_update + lyra_render on the packaged ColdFire code match the host
    reference block for block (before the stock amp chain, stubbed: Unicorn
    has no EMAC), call envelope, VCA and PUNCH in that order, and set the amp
    chain like MACRO/TONE on a trig.
 3. A silent voice (closed envelope, no trig) outputs zeros without rendering.
 4. Cost per block, measured."""
import ctypes,json,os,pathlib,struct,subprocess,tempfile
from unicorn import UC_HOOK_CODE
from unicorn import m68k_const as m
from test_native_live import LiveRig,candidate_meta,SYMBOLS
assert os.environ.get('MC_CANDIDATE'),'packaged candidate required'
D=pathlib.Path(__file__).resolve().parent;P=D.parent;S=SYMBOLS;checks=[]
IMAGE=pathlib.Path(os.environ['MC_CANDIDATE']).with_suffix('.main.bin').read_bytes()
rd=lambda off,n:IMAGE[off:off+n]
u32=lambda off:struct.unpack('>I',rd(off,4))[0]
assert u32(0x1c1f88)==S['lyra_update'] and u32(0x1c1fa4)==S['lyra_render']
assert rd(0x401300ee-0x40000400,6)==b'Lyra\0\0'
for off,name,default in [(0x1c2f90,'spread',0x1400),(0x1c2fc8,'sharp',0x2800),(0x1c3000,'mod',0x1400),(0x1c3038,'hyper',0)]:
 assert u32(off)==5 and u32(off+0x2c)==S['lyra_name_'+name] and u32(off+0x34)==S['lyra_short_'+name] and u32(off+0x10)==default
assert u32(0x1c2fc8+0x0c)==0x7f00
assert rd(0x46ab4,4).hex()=='48787f00' and rd(0x4f900,2).hex()=='7204' and rd(0x4f904,6).hex()=='203c4010a120' and rd(0x5c846,2).hex()=='6010'
checks.append('image: engine entry 5, name Lyra, descriptors (names, defaults, SHAPE 0..127), SHAPE UI as TONE, no CHORD conversion')
# Host reference.
lib=pathlib.Path(tempfile.mkdtemp())/'lyra.dylib'
subprocess.run(['cc','-std=c11','-O1','-shared','-I',str(P/'firmware'),str(P/'firmware/lyra.c'),'-o',str(lib)],check=True)
ref=ctypes.CDLL(str(lib))
class St(ctypes.Structure):_fields_=[('ph',ctypes.c_uint32*4),('y',ctypes.c_int32*4),('lfo',ctypes.c_uint32),('dc_x',ctypes.c_int32),('dc_y',ctypes.c_int32)]
class Pa(ctypes.Structure):_fields_=[('note',ctypes.c_int32),('spread',ctypes.c_uint8),('sharp',ctypes.c_uint8),('mod',ctypes.c_uint8),('hyper',ctypes.c_uint8)]
VOICE0,STRIDE=0x42308828,0x31c
r=LiveRig()
try:r.u.mem_map(0x42300000,0x20000)
except Exception:pass
calls=[];outs=[]
def amp(name):
 def f(a):
  calls.append(name)
  if name=='vca':outs.append(struct.unpack('>32i',bytes(r.u.mem_read(a[0],128))))
  return 0
 return f
for addr,name in ((0x400a9252,'env'),(0x400a9430,'vca'),(0x400a967a,'punch')):r.u.hook_add(UC_HOOK_CODE,r.stub(amp(name)),begin=addr,end=addr)
count=[0];r.u.hook_add(UC_HOOK_CODE,lambda u,a,n,d:count.__setitem__(0,count[0]+1))
OUT,PAR=0x41540000,0x41541000
cost=[]
for case,(pitch,color,shape,sweep,contour,fine,note) in enumerate([(64,0,0,0,0,64,57),(64,30,60,40,0,64,60),(70,100,127,127,90,80,45),(50,64,20,90,127,40,72)]):
 t=case%6;v=VOICE0+STRIDE*t;r.u.mem_write(v,bytes(STRIDE))
 par=bytearray(0x30)
 for off,val in ((0x14,pitch),(0x16,color),(0x18,shape),(0x1a,sweep),(0x1c,contour),(0x1e,1),(0x22,fine),(0x24,50)):par[off:off+2]=struct.pack('>H',val<<8)
 r.u.mem_write(PAR,bytes(par))
 st=St();pa=Pa();pa.note=max(0,min(127<<16,(pitch<<16)+((fine-64)<<11)-(64<<16)+(note<<16)));pa.spread,pa.sharp,pa.mod,pa.hyper=color,shape,sweep,contour
 for blk in range(40):
  r.w32(v+0x34,1 if blk==0 else 0);r.w32(v+0x38,1 if blk==1 else 0)
  if blk>1:r.w32(v+0x230,0x40000000)          # envelope open
  r.call('lyra_update',note<<16,v,PAR)
  if blk==1:assert struct.unpack('>I',bytes(r.u.mem_read(v+0x274,4)))[0]==0x012c0f26 and struct.unpack('>I',bytes(r.u.mem_read(v+0x29c,4)))[0]==0x20000000
  calls.clear();outs.clear();count[0]=0
  r.call('lyra_render',OUT,v);cost.append(count[0])
  assert calls==['env','vca','punch'],calls
  buf=(ctypes.c_int32*32)();ref.lyra_block(ctypes.byref(st),ctypes.byref(pa),buf)
  assert list(outs[0])==list(buf),(case,blk)
checks.append('packaged lyra_update/lyra_render match the host reference block for block, then envelope, VCA, PUNCH; trig sets the TONE amp chain')
# Silent voice.
v=VOICE0+STRIDE*2;r.u.mem_write(OUT,b'\x55'*128);r.w32(v+0x34,0);r.w32(v+0x38,0);r.w32(v+0x230,100);r.w32(v+0x234,100);calls.clear();count[0]=0
r.call('lyra_render',OUT,v);assert calls==[] and bytes(r.u.mem_read(OUT,128))==bytes(128);idle=count[0]
checks.append('silent voice: zeros, no rendering, no amp chain')
report={'result':'PASS','package_sha256':candidate_meta['syx_sha256'],'checks':checks,
 'instructions_per_block':{'sounding_max':max(cost),'sounding_mean':round(sum(cost)/len(cost)),'silent':idle,'note':'excludes the stock amp chain; stock voice ~7600, CHORD ~8600 per block (upstream notes/23)'},
 'limitations':'Amp envelope/VCA/PUNCH stubbed (EMAC); voice loop dispatch and MACHINES screen not executed; no audio heard.'}
(D/'selected-lyra-report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({'result':'PASS','checks':len(checks),'cost':report['instructions_per_block']},indent=2))
