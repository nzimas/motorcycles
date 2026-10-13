#!/usr/bin/env python3
"""Reference implementation of the Motor:Cycles patcher (the macOS app does the
same in Swift): stock Model:Cycles OS 1.13 .syx + motorcycles.mcpatch (format 2)
-> Motor:Cycles .syx, byte for byte the classic two-stage build, verified
against the hashes in the kit at every stage.
Usage: tools/apply_patchkit.py STOCK.syx KIT.mcpatch OUT.syx"""
import hashlib,pathlib,struct,sys
ROOT=pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'reference/Modded-Cycles/tools'))
from mtlib import aplib,container
from mtlib.syx import unwrap,wrap,BYTES_PER_MSG
import aplib_grow
sha=lambda b:hashlib.sha256(b).digest()
def decode(kit,p):
    count=struct.unpack('>I',kit[p:p+4])[0];p+=4;ops=[]
    for _ in range(count):
        kind=kit[p];p+=1
        if kind==0:src,n=struct.unpack('>II',kit[p:p+8]);p+=8;ops.append((0,src,n))
        else:n=struct.unpack('>I',kit[p:p+4])[0];p+=4;ops.append((1,kit[p:p+n]));p+=n
    return ops,p
def build(base,ops):
    out=bytearray()
    for op in ops:out+=base[op[1]:op[1]+op[2]] if op[0]==0 else op[1]
    return bytes(out)
def repack(data,ops,before,ranges=None):
    if ranges is None:
        dirty=bytearray(a!=b for a,b in zip(before,data));dirty.extend(b'\1'*(len(data)-len(before)))
    else:
        dirty=bytearray(len(data))
        for a,n in ranges:dirty[a:a+n]=b'\1'*n
    return aplib_grow.repack_grow(data,ops,dirty,len(before))
def apply(stock_raw,kit):
    assert kit[:4]==b'MCPK','not a Motor:Cycles patch kit'
    version,section=struct.unpack('>HH',kit[4:8]);assert version==2,'patch kit format'
    h=[kit[8+32*i:40+32*i] for i in range(6)]   # stock syx, stock main, baseline main, baseline packed, final main, output
    if sha(stock_raw)!=h[0]:raise ValueError('this is not the official Model:Cycles OS 1.13 firmware')
    stream,transport=unwrap(stock_raw);info=container.parse(stream)
    sec=next(s for s in info['sections'] if s['id']==section)
    stock,ops0=aplib.depack(info['blob'][sec['off']:sec['off']+sec['size']]);assert sha(stock)==h[1]
    len1,len2=struct.unpack('>II',kit[200:208]);r1,p=decode(kit,208);r2,p=decode(kit,p)
    count=struct.unpack('>I',kit[p:p+4])[0];p+=4;ranges=[struct.unpack('>II',kit[p+8*i:p+8*i+8]) for i in range(count)]
    baseline=build(stock,r1);assert len(baseline)==len1 and sha(baseline)==h[2],'baseline image mismatch'
    packed1=repack(baseline,ops0,stock,ranges);assert sha(packed1)==h[3],'baseline packing mismatch'
    base1,ops1=aplib.depack(packed1);assert base1==baseline
    final=build(baseline,r2);assert len(final)==len2 and sha(final)==h[4],'rebuilt OS image mismatch'
    packed2=repack(final,ops1,baseline)
    plains=[]
    for s in info['sections']:
        try:plains.append(aplib.depack(info['blob'][s['off']:s['off']+s['size']])[0])
        except Exception:pass
    key=container.find_key(plains,info['blob'][:-32],info['blob'][-32:]);assert key
    out=wrap(container.build_stream(container.rebuild(info,{section:packed2},key),BYTES_PER_MSG),transport['product'],transport['start_seq'])
    assert sha(out)==h[5],'output package mismatch'
    return out
if __name__=='__main__':
    out=apply(pathlib.Path(sys.argv[1]).read_bytes(),pathlib.Path(sys.argv[2]).read_bytes())
    pathlib.Path(sys.argv[3]).write_bytes(out);print('OK',hashlib.sha256(out).hexdigest())
