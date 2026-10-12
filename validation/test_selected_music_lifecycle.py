#!/usr/bin/env python3
"""Exact selected package stock save + patched global load/default lifecycle."""
import pathlib,json,struct,os
from unicorn import Uc,UC_ARCH_M68K,UC_MODE_BIG_ENDIAN
from unicorn import m68k_const as m
from validate_package import read,sha
D=pathlib.Path(__file__).resolve().parent;C=pathlib.Path(os.environ.get('MC_CANDIDATE',str(D.parent/'build/selected-native/ModelCycles-generative-selected')))
meta,_,plain=read(C.with_suffix('.syx'));manifest=json.loads(C.with_suffix('.manifest.json').read_text());assert sha(plain[3])==manifest['main_sha256'];sym={q[2]:int(q[0],16) for line in C.with_suffix('.symbols.txt').read_text().splitlines() if len(q:=line.split())==3}
u=Uc(UC_ARCH_M68K,UC_MODE_BIG_ENDIAN);u.ctl_set_cpu_model(m.UC_CPU_M68K_CFV4E)
for base,size in [(0x40000000,0x2400000),(0x43000000,0x100000),(0x44000000,0x10000)]:u.mem_map(base,size)  # MACRO + native runtime above it
u.mem_write(0x40000400,plain[3]);payload=C.with_suffix('.payload.bin').read_bytes();assert sha(payload)==manifest['payload_sha256'];u.mem_write(manifest['payload_start'],payload)
SP,STOP=0x4400ff00,0x44000000;BANK,FILES,LOAD=0x41000000,0x41400000,0x41800000
w=lambda a,v:u.mem_write(a,struct.pack('>I',v&0xffffffff))
def call(addr,*args):
 u.mem_write(SP,struct.pack('>I'+'I'*len(args),STOP,*args));u.reg_write(m.UC_M68K_REG_SR,0x2000);u.reg_write(m.UC_M68K_REG_A7,SP);u.emu_start(addr,STOP,count=100000);assert u.reg_read(m.UC_M68K_REG_PC)==STOP;assert u.reg_read(m.UC_M68K_REG_A7)==SP+4;return u.reg_read(m.UC_M68K_REG_D0)
# mc_music_read returns uint8_t: the ABI leaves D0 bits8..31 undefined.
checks=[];settings=[]
for p in range(96):
 pattern=BANK+30710*p;globaldata=pattern+30642;file=FILES+14800*p;dest=LOAD+68*p;music=128|((p//12%4)<<4)|(p%12);settings.append(music)
 data=bytearray(b'\xa5'*68);data[:16]=b'User-NAME-12345\0';data[18:20]=(14400).to_bytes(2,'big');data[20:24]=b'\x00\x20\x00\x20';data[24]=0;data[25]=0;data[26]=2;data[27]=0
 u.mem_write(globaldata,bytes(data));assert call(sym['mc_music_write'],globaldata,music)==1;assert bytes(u.mem_read(globaldata,40))==bytes(data[:40]);assert bytes(u.mem_read(globaldata+48,20))==bytes(data[48:]);u.reg_write(m.UC_M68K_REG_A2,file);u.reg_write(m.UC_M68K_REG_A3,pattern);u.reg_write(m.UC_M68K_REG_A7,SP);u.emu_start(0x4005ba26,0x4005ba3c,count=10000);assert bytes(u.mem_read(file+14736,64))==bytes(u.mem_read(globaldata,64))
 u.mem_write(dest,b'\x5a'*68);assert call(0x4005b3b4,dest,file+14736)==1;assert (call(sym['mc_music_read'],dest)&255)==music;assert bytes(u.mem_read(dest,16))==data[:16];checks.append('pattern%d exact save/load and name preservation'%p)
for p in range(96):assert (call(sym['mc_music_read'],LOAD+68*p)&255)==settings[p]
# Copy/paste uses the same canonical global loader and carries owned metadata.
assert call(0x4005b3b4,LOAD+68*95,FILES+14736)==1;assert (call(sym['mc_music_read'],LOAD+68*95)&255)==settings[0];assert (call(sym['mc_music_read'],BANK+30710*95+30642)&255)==settings[95];checks.append('copy/paste carries source setting without changing source/other patterns')
for i in range(8):
 src=FILES+14736;old=bytes(u.mem_read(src+40,8));bad=bytearray(old);bad[i]^=1;u.mem_write(src+40,bytes(bad));assert call(0x4005b3b4,LOAD,src)==1;assert (call(sym['mc_music_read'],LOAD)&255)==0x99 and not any(u.mem_read(LOAD+40,8));u.mem_write(src+40,old)
checks.append('eight independent metadata byte corruptions load A minor default')
# Exact patched stock clear default resets extension; keep mode retains it.
dest=LOAD+68;old=bytes(u.mem_read(dest,68));call(0x40061526,dest,1);assert bytes(u.mem_read(dest,68))==old;call(0x40061526,dest,0);assert not any(u.mem_read(dest+40,8)) and (call(sym['mc_music_read'],dest)&255)==0x99;checks.append('stock keep/default hook retains or resets metadata')
report={'result':'PASS','package_sha256':meta['syx_sha256'],'checks':checks,'limitations':'Exact packaged stock serializer copy and patched global load/default. Synthetic initialized pattern bank and file buffers; not filesystem powercycle or project I/O.'}
(D/'selected-music-lifecycle-report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
