#!/usr/bin/env python3
"""Reference implementation of the Motor:Cycles patcher (the macOS app does the
same in Swift): stock Model:Cycles OS 1.13 .syx + motorcycles.mcpatch ->
Motor:Cycles .syx, verified against the hashes in the kit.
Usage: tools/apply_patchkit.py STOCK.syx KIT.mcpatch OUT.syx"""
import hashlib,pathlib,struct,sys
ROOT=pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'reference/Modded-Cycles/tools'))
from mtlib import aplib,container
from mtlib.syx import unwrap,wrap,BYTES_PER_MSG
import aplib_grow
sha=lambda b:hashlib.sha256(b).digest()
def apply(stock_raw,kit):
    assert kit[:4]==b'MCPK','not a Motor:Cycles patch kit'
    version,section=struct.unpack('>HH',kit[4:8]);assert version==1
    h_syx,h_stock,h_final,h_out=(kit[8+32*i:40+32*i] for i in range(4))
    if sha(stock_raw)!=h_syx:raise ValueError('this is not the official Model:Cycles OS 1.13 firmware')
    stream,transport=unwrap(stock_raw);info=container.parse(stream)
    sec=next(s for s in info['sections'] if s['id']==section)
    stock,ops=aplib.depack(info['blob'][sec['off']:sec['off']+sec['size']]);assert sha(stock)==h_stock
    length,count=struct.unpack('>II',kit[136:144]);p=144;image=bytearray()
    for _ in range(count):
        kind=kit[p];p+=1
        if kind==0:src,n=struct.unpack('>II',kit[p:p+8]);p+=8;image+=stock[src:src+n]
        else:n=struct.unpack('>I',kit[p:p+4])[0];p+=4;image+=kit[p:p+n];p+=n
    assert len(image)==length and sha(image)==h_final,'rebuilt OS image mismatch'
    dirty=bytearray(a!=b for a,b in zip(stock,image));dirty.extend(b'\1'*(len(image)-len(stock)))
    packed=aplib_grow.repack_grow(bytes(image),ops,dirty,len(stock))
    plains=[]
    for s in info['sections']:
        try:plains.append(aplib.depack(info['blob'][s['off']:s['off']+s['size']])[0])
        except Exception:pass
    key=container.find_key(plains,info['blob'][:-32],info['blob'][-32:]);assert key
    out=wrap(container.build_stream(container.rebuild(info,{section:packed},key),BYTES_PER_MSG),transport['product'],transport['start_seq'])
    assert sha(out)==h_out,'output package mismatch'
    return out
if __name__=='__main__':
    out=apply(pathlib.Path(sys.argv[1]).read_bytes(),pathlib.Path(sys.argv[2]).read_bytes())
    pathlib.Path(sys.argv[3]).write_bytes(out);print('OK',hashlib.sha256(out).hexdigest())
