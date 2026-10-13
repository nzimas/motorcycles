#!/usr/bin/env python3
"""Build the Motor:Cycles patch kit (format 2): the recipe that turns a user's
own stock Model:Cycles OS 1.13 .syx into Motor:Cycles, byte for byte the file
produced by the classic two-stage build and tested on hardware, without
containing Elektron code.

Stage 1 rebuilds the Modded-Cycles baseline MAIN (the MIT-licensed tweaks) as
COPY/BYTES over the stock MAIN and repacks it with the stock section's own
aPLib op stream (repack_grow), exactly as Modded-Cycles' build.py does.
Stage 2 rebuilds the Motor:Cycles MAIN as COPY/BYTES over that baseline and
repacks it with the baseline section's op stream, exactly as
firmware/build-selected.py --base does. Every intermediate is hashed.

Usage: tools/make_patchkit.py STOCK.syx BASELINE.syx BUILD_DIR OUT.mcpatch
  BUILD_DIR: firmware/build-selected.py --base BASELINE.syx output.
"""
import hashlib,json,pathlib,struct,sys
ROOT=pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'reference/Modded-Cycles/tools'))
from mtlib import aplib,container
from mtlib.syx import unwrap
MAGIC=b'MCPK';VERSION=2;WINDOW=12
sha=lambda b:hashlib.sha256(b).digest()
def section3(path):
    raw=path.read_bytes();stream,_=unwrap(raw);info=container.parse(stream)
    sec=next(s for s in info['sections'] if s['id']==3);packed=info['blob'][sec['off']:sec['off']+sec['size']]
    return raw,packed,aplib.depack(packed)[0]
def recipe(base,target):
    """target as COPY (offset, length) from base, or literal BYTES."""
    index={}
    for i in range(0,len(base)-WINDOW+1):index.setdefault(base[i:i+WINDOW],i)
    ops=[];lit=bytearray()
    def flush():
        if lit:ops.append((1,bytes(lit)));lit.clear()
    i=0
    while i<len(target):
        if i<len(base) and target[i]==base[i]:
            j=i
            while j<len(target) and j<len(base) and target[j]==base[j]:j+=1
            if j-i>=4:flush();ops.append((0,i,j-i));i=j;continue
        src=index.get(target[i:i+WINDOW])
        if src is not None:
            n=WINDOW
            while i+n<len(target) and src+n<len(base) and target[i+n]==base[src+n]:n+=1
            flush();ops.append((0,src,n));i+=n;continue
        lit.append(target[i]);i+=1
    flush()
    merged=[]
    for op in ops:
        if merged and op[0]==0 and merged[-1][0]==0 and merged[-1][1]+merged[-1][2]==op[1]:merged[-1]=(0,merged[-1][1],merged[-1][2]+op[2])
        else:merged.append(op)
    return merged
def encode(ops):
    out=bytearray(struct.pack('>I',len(ops)))
    for op in ops:
        if op[0]==0:out+=struct.pack('>BII',0,op[1],op[2])
        else:out+=struct.pack('>BI',1,len(op[1]))+op[1]
    return bytes(out)
def baseline_dirty(stock,baseline):
    """The bytes Modded-Cycles' build.py marks dirty for the baseline: every
    byte a selected tweak writes (changed or not, same rule as apply_writes),
    plus the appended payload. Returned as (start, length) ranges."""
    config=json.loads((ROOT/'firmware/selected-configuration.json').read_text())
    tweaks={}
    for f in sorted((ROOT/'reference/Modded-Cycles/tweaks/model-cycles_OS1.13').glob('[0-9]*.json')):
        t=json.loads(f.read_text());tweaks[t['id']]=t
    chosen=sorted((tweaks[i] for i in config['upstream_tweak_ids']),key=lambda t:t['order'])
    data=bytearray(stock);dirty=bytearray(len(stock))
    for t in chosen:
        for w in t['writes']:
            off=w['off'];old=bytes.fromhex(w['old']);new=bytes.fromhex(w['new']);cur=bytes(data[off:off+len(old)])
            if cur==new and cur!=old:continue
            assert cur==old,(t['id'],off)
            data[off:off+len(new)]=new
            for k in range(off,off+len(new)):dirty[k]=1
    assert bytes(data)==baseline[:len(stock)],'tweak writes do not reproduce the baseline'
    dirty.extend(b'\1'*(len(baseline)-len(stock)))
    ranges=[];i=0
    while i<len(dirty):
        if dirty[i]:
            j=i
            while j<len(dirty) and dirty[j]:j+=1
            ranges.append((i,j-i));i=j
        else:i+=1
    return ranges
def main():
    stock_path,baseline_path,build,out=(pathlib.Path(a) for a in sys.argv[1:5])
    stock_raw,stock_packed,stock=section3(stock_path)
    baseline_raw,baseline_packed,baseline=section3(baseline_path)
    stem=build/'ModelCycles-generative-selected'
    final=stem.with_suffix('.main.bin').read_bytes();package=stem.with_suffix('.syx').read_bytes()
    manifest=json.loads(stem.with_suffix('.manifest.json').read_text())
    assert sha(final).hex()==manifest['main_sha256'] and sha(package).hex()==manifest['package_sha256']
    assert sha(baseline).hex()==manifest['baseline_main_sha256'],'build was not made on this baseline'
    r1=recipe(stock,baseline);r2=recipe(baseline,final)
    body=bytearray(MAGIC+struct.pack('>HH',VERSION,3))
    body+=sha(stock_raw)+sha(stock)+sha(baseline)+sha(baseline_packed)+sha(final)+sha(package)
    body+=struct.pack('>II',len(baseline),len(final))
    ranges=baseline_dirty(stock,baseline)
    body+=encode(r1)+encode(r2)+struct.pack('>I',len(ranges))+b''.join(struct.pack('>II',a,n) for a,n in ranges)
    out.write_bytes(bytes(body))
    lit=lambda r:sum(len(op[1]) for op in r if op[0]==1)
    print(json.dumps({'dirty_ranges':len(ranges),'stage1_ops':len(r1),'stage1_literal_bytes':lit(r1),'stage2_ops':len(r2),'stage2_literal_bytes':lit(r2),'kit_bytes':len(body),'final_syx_sha256':sha(package).hex()}))
if __name__=='__main__':main()
