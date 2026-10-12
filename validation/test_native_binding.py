#!/usr/bin/env python3
"""Verify compiled resolver after actual stock pointer binding and field setters."""
import pathlib,json,hashlib,struct
from test_native_runtime import Rig,SYM,D,ROOT,VTABLE,SOUND
from validate_package import read
STOCK='/Users/nzimas/Downloads/model-cycles_OS1.13/model-cycles_OS1.13.syx'
meta,stored,plain=read(STOCK)
r=Rig();r.u.mem_write(0x40000400,plain[3]);OWNER,PATTERN,OUT=0x41120000,0x41130000,0x41180000
notify=[]
r.u.hook_add(4,r.stub(lambda args:notify.append(args[0]) or 0),begin=0x40015020,end=0x40015020)
r.u.hook_add(4,r.stub(lambda args:0),begin=0x400d0f3c,end=0x400d0f3c)
r.w32(VTABLE+16,0x40015020);r.w32(VTABLE+36,0x400d2a8e);r.w32(VTABLE+40,0x400d2aa4)
for owner in [OWNER,OWNER+44,OWNER+640]+[OWNER+112+88*i for i in range(6)]:r.w32(owner,VTABLE)
for i in range(6):r.w32(ROOT+212+68*i+16,SOUND+100*i)
SYM.update(stock_assign=0x4000c5d0,set_parent=0x400157aa,set_lock=0x400157bc,set_index=0x400157c8)
for i in range(6):
 owner=OWNER+112+88*i;r.call('set_parent',owner,OWNER);r.call('set_lock',owner,OWNER+640);r.call('set_index',owner,i)
r.call('stock_assign',OWNER,PATTERN)
assert len(notify)==9 # pattern+global+6tracks+lock
assert r.value('mc_native')>=0 # no mutation assertion; simply check target symbol readable
r.u.mem_write(OUT,b'\xa5'*120)
assert r.call('mc_binding_resolve',OUT,ROOT,OWNER,SYM['mc_fw_owner_data'])==1
resolved=struct.unpack('>30I',r.u.mem_read(OUT,120))
assert resolved[:3]==(OWNER,OWNER+44,OWNER+640)
assert resolved[3:9]==tuple(OWNER+112+88*i for i in range(6))
assert resolved[9:15]==tuple(ROOT+212+68*i for i in range(6))
assert resolved[15:18]==(PATTERN,PATTERN+30642,PATTERN+4332)
assert resolved[18:24]==tuple(PATTERN+722*i for i in range(6))
assert resolved[24:30]==tuple(SOUND+100*i for i in range(6))
checks=['actual stock pointerassignment/9notifications and fieldsetter layout','compiled resolver all30canonical pointers']
def reject(name,owner,offset,bad):
 old=int.from_bytes(r.u.mem_read(owner+offset,4),'big');r.w32(owner+offset,bad);r.u.mem_write(OUT,b'\xa5'*120)
 assert not r.call('mc_binding_resolve',OUT,ROOT,OWNER,SYM['mc_fw_owner_data']),name
 assert bytes(r.u.mem_read(OUT,120))==b'\xa5'*120,name+' altered output';r.w32(owner+offset,old);checks.append(name)
reject('inconsistent globalalias',OWNER+44,16,PATTERN+30640)
reject('inconsistent lockalias',OWNER+640,16,PATTERN+4334)
reject('incorrect trackindex',OWNER+112+88*3,56,4)
reject('incorrect trackparent',OWNER+112+88*2,60,ROOT)
reject('incorrect lockowner',OWNER+112+88*4,44,ROOT)
reject('null sounddata',ROOT+212+68*5,16,0)
reject('pattern pointeroverflow',OWNER,16,0xfffffff0)
report={'result':'PASS','checks':checks,'stock_sha256':meta['syx_sha256'],'elf_sha256':hashlib.sha256((D/'native-runtime.elf').read_bytes()).hexdigest(),'limitations':'Actual stock c5d0 pointerassignment and157aa/bc/c8 fieldsetters plus compiled resolver; synthetic owner vtables and notifications. No actual registered audioobservers or publication.'}
(D/'native-binding-report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
