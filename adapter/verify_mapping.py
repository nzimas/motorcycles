#!/usr/bin/env python3
"""Verify control mappings against user-supplied official OS; no device access."""
import argparse, hashlib, json, pathlib, struct, sys
ROOT=pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'reference/Modded-Cycles/tools'))
from mtlib import aplib, container
from mtlib.syx import unwrap
BASE=0x40000400
STOCK='44fe586269631a0ca7da25a3383fc6733c314809505fc3cc52f1e0ed9800640c'
def verify(path):
 raw=pathlib.Path(path).read_bytes()
 assert hashlib.sha256(raw).hexdigest()==STOCK,'requires exact official OS1.13'
 stream,_=unwrap(raw); c=container.parse(stream); s=next(s for s in c['sections'] if s['id']==3)
 main=aplib.depack(c['blob'][s['off']:s['off']+s['size']])[0]
 def at(a,n):return main[a-BASE:a-BASE+n]
 def words(a,n):return struct.unpack('>'+str(n)+'I',at(a,n*4))
 def string(a):return main[a-BASE:main.index(0,a-BASE)].decode('ascii')
 pads=struct.unpack('>7i',at(0x401001d0,28));assert pads==(-1,0,1,2,3,4,5)
 assert at(0x4001d198,6)==bytes.fromhex('4eb9400740ca')
 assert at(0x400740ca,8)==bytes.fromhex('206f000420280014')
 assert at(0x400740d4,10)==bytes.fromhex('206f00047001b0a80010')
 assert at(0x4000614c,12)==bytes.fromhex('7040d1af00044ef94007746c')
 assert at(0x4001e828,2)==bytes.fromhex('5583')
 # Machine0 row starts 0x40a7158c; first two assignments establish IDs42/50.
 assert at(0x400e1028,10)==bytes.fromhex('722a703223c140a71594')
 assert at(0x400e1036,6)==bytes.fromhex('23c040a71598')
 controls=[]
 for idx,param,role in [(2,42,'density'),(3,50,'brutality')]:
  r=words(0x4010dce0+param*56,14)
  controls.append(dict(encoder_index=idx,parameter_id=param,name=string(r[11]),short_name=string(r[13]),slot=r[1],minimum=r[2]/256,maximum=r[3]/256,generator_control=role))
 assert controls[0]['name']=='Pitch' and controls[1]['name']=='Amp Decay'
 return dict(stock_sha256=STOCK,pad_event=dict(dispatch='0x4000614c',index_offset=20,physical_index=list(range(1,7)),track_mapping=list(pads[1:]),state_offset=16,press=1,release=0,velocity_offset=24),controls=controls,evidence='stock bytes and descriptor contents; no device validation')
if __name__=='__main__':
 ap=argparse.ArgumentParser();ap.add_argument('firmware');ap.add_argument('--output');args=ap.parse_args()
 result=json.dumps(verify(args.firmware),indent=2)+'\n'
 if args.output:pathlib.Path(args.output).write_text(result)
 else:print(result,end='')
