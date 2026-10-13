#!/usr/bin/env python3
"""Stock-era sounds (RC22): the stock OS reads a track's machine from the high
byte of its 8.8 word (sound+38) and ignores the low byte (sound+39), which
sounds made before Motor:Cycles can carry. RC21 took a non-zero low byte for an
unknown machine and every generator refused the whole pattern.
 1. The payload differs from RC21's by the four bytes of the in-place fix
    (mc_read_machines), so no code moves.
 2. With a non-zero low byte on a track's machine word, Patch, Kit, Phrase and
    Pattern all generate, and the machine read is the high byte."""
import contextlib,io,json,os,pathlib
assert os.environ.get('MC_CANDIDATE'),'packaged candidate required'
with contextlib.redirect_stdout(io.StringIO()):
    from test_native_live import LiveRig,SOUND,SYMBOLS as S,candidate_meta
D=pathlib.Path(__file__).resolve().parent;checks=[]
C=pathlib.Path(os.environ['MC_CANDIDATE'])
payload=C.with_suffix('.payload.bin').read_bytes()
rc21=D.parent/'build/rc21/ModelCycles-generative-selected.payload.bin'
if rc21.exists():
    old=rc21.read_bytes();assert len(old)==len(payload)
    diff=[i for i in range(len(old)) if old[i]!=payload[i]]
    at=S['mc_read_machines']-0x43060000
    assert diff==list(range(diff[0],diff[0]+4)) and at<diff[0]<at+0x100,diff
    assert old[diff[0]:diff[0]+4].hex()=='7f814a01' and payload[diff[0]:diff[0]+4].hex()=='7e004e71'
    checks.append('payload = RC21 except the 4-byte in-place fix in mc_read_machines (no code moves)')
def attempt(g,low,track=4):
    r=LiveRig()
    if low is not None:r.u.mem_write(SOUND+100*track+39,bytes((low,)))
    old=r.capture()
    r.key(15,1);r.pad(g+1,1);r.pad(g+1,0);r.key(31,1);r.advance(1500);r.key(31,8);r.key(31,16);r.key(15,16)
    machines=bytes(r.u.mem_read(S['mc_native']+0x1f4d,6))
    return r.capture()!=old,machines
for g,name in enumerate(['Patch','Kit','Phrase','Pattern']):
    for low in (0x01,0x80,0xff):
        made,machines=attempt(g,low)
        assert made,(name,low)
    checks.append('%s generates with a stock-era machine word (low byte 0x01/0x80/0xff on a track)'%name)
_,machines=attempt(1,None);r=LiveRig();r.u.mem_write(SOUND+100*4+39,b'\x80');r.call('mc_read_machines')
assert bytes(r.u.mem_read(S['mc_native']+0x1f4d,6))==bytes(range(6)) and r.u.mem_read(S['mc_native']+0x1f4c,1)[0]==63
checks.append('mc_read_machines: every track valid, machine = high byte (as the stock OS reads it)')
report={'result':'PASS','package_sha256':candidate_meta['syx_sha256'],'checks':checks,
 'limitations':'Synthetic sounds with clean values plus the stock-era low byte; real stock projects not loaded.'}
(D/'selected-stock-sounds-report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({'result':'PASS','checks':len(checks)},indent=2))
