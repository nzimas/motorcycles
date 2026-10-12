#!/usr/bin/env python3
"""Independent offline package integrity and protected-section validation."""
import argparse, hashlib, hmac, json, pathlib, sys
ROOT=pathlib.Path(__file__).resolve().parents[1]
UP=ROOT/'reference/Modded-Cycles'
sys.path.insert(0,str(UP/'tools'))
from mtlib import aplib, container
from mtlib.syx import unwrap, wrap

def sha(b): return hashlib.sha256(b).hexdigest()
def read(path):
    raw=pathlib.Path(path).read_bytes(); stream, transport=unwrap(raw); c=container.parse(stream)
    assert len(c['blob'])==c['declared'], 'truncated container'
    assert int.from_bytes(stream[4:8],'big')==container.content_checksum(c['blob']), 'content checksum'
    assert len({s['id'] for s in c['sections']})==len(c['sections']), 'duplicate section'
    stored={}; plain={}
    for s in c['sections']:
        assert s['off']%16==0 and s['off']>=container.TABLE_OFF+len(c['sections'])*container.ENTRY_SZ, 'section alignment/table overlap'
        assert s['off']+s['size']<=len(c['blob'])-32, 'section exceeds payload'
        stored[s['id']]=c['blob'][s['off']:s['off']+s['size']]
        try: plain[s['id']]=aplib.depack(stored[s['id']])[0]
        except Exception: pass
    spans=sorted((s['off'],s['off']+s['size']) for s in c['sections'])
    assert all(a[1]<=b[0] for a,b in zip(spans,spans[1:])), 'overlapping sections'
    assert container.find_key(list(plain.values()),c['blob'][:-32],c['blob'][-32:]), 'HMAC verification'
    assert wrap(stream,transport['product'],transport['start_seq'])==raw, 'transport roundtrip'
    return {'syx_sha256':sha(raw),'syx_bytes':len(raw),'transport':transport,'vendor_version':c['version'],'sections':[dict(s,stored_sha256=sha(stored[s['id']]),plain_sha256=sha(plain[s['id']]) if s['id'] in plain else None,plain_bytes=len(plain[s['id']]) if s['id'] in plain else None) for s in c['sections']]},stored,plain

def main():
    p=argparse.ArgumentParser();p.add_argument('stock');p.add_argument('--candidate');p.add_argument('--out');a=p.parse_args()
    report,stored,plain=read(a.stock)
    device=json.loads((UP/'tweaks/model-cycles_OS1.13/device.json').read_text())
    assert report['syx_sha256']==device['stock_syx_sha256'],'stock whole-file hash'
    assert sha(plain[3])==device['section_sha256'] and len(plain[3])==device['section_len'],'stock MAIN OS'
    result={'stock':report,'checks':['packet checksums','content checksum','HMAC','section bounds and non-overlap','exact transport roundtrip','pinned stock hash','pinned MAIN OS hash']}
    if a.candidate:
        candidate,cs,cp=read(a.candidate)
        assert candidate['vendor_version']==report['vendor_version'],'vendor version changed'
        assert cs.keys()==stored.keys(),'section set changed'
        assert all(cs[k]==stored[k] for k in stored if k!=3),'protected section changed'
        for s,t in zip(report['sections'],candidate['sections']):
            assert (s['id'],s['attr'])==(t['id'],t['attr']),'section identity/attributes changed'
        result.update(candidate=candidate,protected_sections=[k for k in stored if k!=3])
        result['checks']+=['vendor version preserved','protected stored sections identical','section attributes preserved']
    text=json.dumps(result,indent=2)+'\n'
    if a.out: pathlib.Path(a.out).write_text(text)
    print(text)
if __name__=='__main__':main()
