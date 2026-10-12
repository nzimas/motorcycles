#!/usr/bin/env python3
"""Create an offline stock-baseline integration fixture, never a release.

Does not identify or preserve the user's installed modification configuration.
No MIDI/device access. Requires an explicit reference-only flag.
"""
import argparse, hashlib, json, pathlib, sys
ROOT=pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'reference/Modded-Cycles/tools'))
from mtlib import aplib,container
from mtlib.syx import unwrap,wrap,BYTES_PER_MSG
import aplib_grow
BASE=0x40000400
STOCK='44fe586269631a0ca7da25a3383fc6733c314809505fc3cc52f1e0ed9800640c'
def sha(b):return hashlib.sha256(b).hexdigest()
def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--reference-only',action='store_true',required=True)
    p.add_argument('--stock',required=True);p.add_argument('--manifest',required=True)
    p.add_argument('--payload',required=True);p.add_argument('--out',required=True)
    args=p.parse_args();raw=pathlib.Path(args.stock).read_bytes()
    assert sha(raw)==STOCK,'requires exact official stock baseline'
    manifest=json.loads(pathlib.Path(args.manifest).read_text())
    stream,transport=unwrap(raw);c=container.parse(stream)
    s3=next(s for s in c['sections'] if s['id']==3)
    old,ops=aplib.depack(c['blob'][s3['off']:s3['off']+s3['size']])
    patched=bytearray(old);dirty=bytearray(len(old))
    # Manifest schema is normalized by the native builder. Each entire replaced
    # instruction span is checked against the original before any output exists.
    writes=manifest['patches'];seen=set()
    for entry in writes:
        address=entry['address'];before=bytes.fromhex(entry['old_hex']);after=bytes.fromhex(entry['new_hex'])
        offset=address-BASE
        assert len(before)==len(after) and 0<=offset<=len(old)-len(before)
        assert old[offset:offset+len(before)]==before,'unexpected hook bytes'
        for i,value in enumerate(after):
            assert offset+i not in seen,'overlapping manifest writes'
            seen.add(offset+i);patched[offset+i]=value;dirty[offset+i]=1
    payload=pathlib.Path(args.payload).read_bytes()
    assert sha(payload)==manifest['payload_sha256'],'payload does not match manifest'
    start=manifest['payload_start'];end=manifest['payload_memory_end']
    assert start==0x401ab750 and start%16==0 and end%16==0
    assert BASE+len(old)<=start and start+len(payload)<=end<0x401eb750
    extension=b'\0'*(start-BASE-len(old))+payload+b'\0'*(end-start-len(payload))
    patched+=extension;dirty+=b'\x01'*len(extension)
    assert sha(bytes(patched))==manifest['reference_main_sha256'],'linked MAIN differs'
    packed=aplib_grow.repack_grow(bytes(patched),ops,dirty,len(old))
    assert aplib.depack(packed)[0]==bytes(patched),'compression roundtrip'
    plain=[]
    for section in c['sections']:
        try:plain.append(aplib.depack(c['blob'][section['off']:section['off']+section['size']])[0])
        except Exception:pass
    key=container.find_key(plain,c['blob'][:-32],c['blob'][-32:])
    assert key,'original package integrity'
    blob=container.rebuild(c,{3:packed},key)
    output=wrap(container.build_stream(blob,BYTES_PER_MSG),transport['product'],transport['start_seq'])
    destination=pathlib.Path(args.out)
    assert 'reference' in destination.name,'fixture filename must identify reference status'
    destination.parent.mkdir(parents=True,exist_ok=True);destination.write_bytes(output)
    report={'status':'OFFLINE REFERENCE FIXTURE — NOT A RELEASE','installed_modification_configuration':None,
            'preserves_installed_modifications':False,'package_sha256':sha(output),'main_sha256':sha(bytes(patched)),
            'main_bytes':len(patched),'payload_start':hex(start),'reservation_end':hex(end),
            'limitations':'Stock baseline only. Do not transfer to the instrument. Final configuration, full boot and ISR deadline checks remain release gates.'}
    destination.with_suffix('.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2))
if __name__=='__main__':main()
