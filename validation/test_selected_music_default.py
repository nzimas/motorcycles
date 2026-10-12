#!/usr/bin/env python3
"""Exact packaged runtime: with no scale chosen for the pattern the generator
receives music 0 (kick root C) while the LEDs keep the A natural minor default;
a chosen scale reaches both unchanged."""
import json,os,pathlib
from test_native_live import LiveRig,PATTERN,candidate_meta
assert os.environ.get('MC_CANDIDATE'),'packaged candidate required'
D=pathlib.Path(__file__).resolve().parent
MUSIC,UI_MUSIC=8028,59   # offsetof(mc_controller,music), offsetof(mc_controller,ui.music) on ColdFire
checks=[]
for tag,want,ui in ((b'\x00'*8,0,0x99),(b'MCG4\x01\x95'+bytes((0x95^0xa5,(0x95+0x37)&255)),0x95,0x95),(b'MCG4\x01\x95\x00\x00',0,0x99)):
 r=LiveRig();r.u.mem_write(PATTERN+30642+40,tag);r.call('mc_read_machines')
 got=r.u.mem_read(r.symbols['mc_native']+MUSIC,1)[0];shown=r.u.mem_read(r.symbols['mc_native']+UI_MUSIC,1)[0]
 assert (got,shown)==(want,ui),(tag.hex(),got,shown);checks.append('tag %s -> generator 0x%02x, LEDs 0x%02x'%(tag.hex(),got,shown))
report={'result':'PASS','package_sha256':candidate_meta['syx_sha256'],'checks':checks,'limitations':'Synthetic owner tree; mc_read_machines run on the packaged payload.'}
(D/'selected-music-default-report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
