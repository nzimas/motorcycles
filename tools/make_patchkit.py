#!/usr/bin/env python3
"""Build the Motor:Cycles patch kit: the recipe that turns a user's own stock
Model:Cycles OS 1.13 .syx into Motor:Cycles, without containing Elektron code.

The recipe rebuilds the decompressed MAIN OS (section 3) as a sequence of
  COPY  (offset, length) from the user's stock MAIN, or
  BYTES (literal bytes): our payload, our patches and the MIT-licensed
        Modded-Cycles tweak bytes.
Literal runs are searched against the stock MAIN first (12-byte windows), so
anything the build copied from the stock image is expressed as a COPY.
The patcher then repacks with the stock section's own aPLib op stream
(aplib_grow.repack_grow), re-signs with the key recovered from the user's file,
and must reproduce every hash recorded here.

Usage: tools/make_patchkit.py STOCK.syx BUILD_DIR OUT.mcpatch
  BUILD_DIR: firmware/build-selected.py --stock output.
"""
import hashlib,json,pathlib,struct,sys
ROOT=pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'reference/Modded-Cycles/tools'))
from mtlib import aplib,container
from mtlib.syx import unwrap
MAGIC=b'MCPK';VERSION=1;WINDOW=12
sha=lambda b:hashlib.sha256(b).digest()
def main():
    stock_path,build,out=pathlib.Path(sys.argv[1]),pathlib.Path(sys.argv[2]),pathlib.Path(sys.argv[3])
    stock_raw=stock_path.read_bytes()
    stream,transport=unwrap(stock_raw);info=container.parse(stream)
    sec=next(s for s in info['sections'] if s['id']==3)
    stock=aplib.depack(info['blob'][sec['off']:sec['off']+sec['size']])[0]
    stem=build/'ModelCycles-generative-selected'
    final=stem.with_suffix('.main.bin').read_bytes();package=stem.with_suffix('.syx').read_bytes()
    manifest=json.loads(stem.with_suffix('.manifest.json').read_text())
    assert sha(final).hex()==manifest['main_sha256'] and sha(package).hex()==manifest['package_sha256']
    index={}
    for i in range(0,len(stock)-WINDOW+1):index.setdefault(stock[i:i+WINDOW],i)
    ops=[];lit=bytearray()
    def flush():
        if lit:ops.append((1,bytes(lit)));lit.clear()
    i=0
    while i<len(final):
        if i<len(stock) and final[i]==stock[i]:
            j=i
            while j<len(final) and j<len(stock) and final[j]==stock[j]:j+=1
            if j-i>=4:
                flush();ops.append((0,i,j-i));i=j;continue
        src=index.get(final[i:i+WINDOW])
        if src is not None:
            n=WINDOW
            while i+n<len(final) and src+n<len(stock) and final[i+n]==stock[src+n]:n+=1
            flush();ops.append((0,src,n));i+=n;continue
        lit.append(final[i]);i+=1
    flush()
    # Merge adjacent copies.
    merged=[]
    for op in ops:
        if merged and op[0]==0 and merged[-1][0]==0 and merged[-1][1]+merged[-1][2]==op[1]:merged[-1]=(0,merged[-1][1],merged[-1][2]+op[2])
        else:merged.append(op)
    body=bytearray(MAGIC+struct.pack('>HH',VERSION,3))
    body+=sha(stock_raw)+sha(stock)+sha(final)+sha(package)
    body+=struct.pack('>II',len(final),len(merged))
    literal_bytes=0
    for op in merged:
        if op[0]==0:body+=struct.pack('>BII',0,op[1],op[2])
        else:body+=struct.pack('>BI',1,len(op[1]))+op[1];literal_bytes+=len(op[1])
    out.write_bytes(bytes(body))
    print(json.dumps({'ops':len(merged),'literal_bytes':literal_bytes,'copied_bytes':len(final)-literal_bytes,'kit_bytes':len(body),'final_syx_sha256':sha(package).hex()}))
if __name__=='__main__':main()
