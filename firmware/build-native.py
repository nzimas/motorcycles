#!/usr/bin/env python3
"""Build a stock-reference-only native MAIN/ELF; never produces SysEx or deploys.
Installed modification configuration is deliberately not inferred. Use emitted
manifest as evidence for a later exact selected-modification packager."""
import argparse,hashlib,json,pathlib,struct,subprocess,sys
ROOT=pathlib.Path(__file__).resolve().parents[1]
BASE=0x40000400;PAYLOAD=0x401ab750;CACHE_ENTRY=0x401eb750;BLOCK=16384
OFFICIAL='44fe586269631a0ca7da25a3383fc6733c314809505fc3cc52f1e0ed9800640c'
SOURCES=['generation/music.c','generation/generation.c','generation/runtime.c','generation/sequence_encoding.c',
 'interface/interface.c','interface/controller.c','adapter/firmware.c','adapter/fast_blocks.S',
 'firmware/binding.c','firmware/duration.c','firmware/preparation.c','firmware/live.c',
 'firmware/runtime.c','firmware/hooks.S','firmware/popup.S','firmware/music_storage.c','firmware/music_hooks.S']
# Complete instruction spans, with replay continuations in hooks.S.
HOOKS=[(0x400060fe,8,'mc_key_hook'),(0x4000614c,6,'mc_pad_hook'),
 (0x40006158,8,'mc_encoder_hook'),(0x40005f86,8,'mc_light_hook'),
 (0x4005b3b4,8,'mc_music_load_hook'),(0x40061526,6,'mc_music_clear_hook'),
 (0x4000053a,6,'mc_boot_init_hook'),(0x400004d2,8,'mc_clear_hook')]
EXPECTED={0x4005b3b4:'4feffff448d70c04',0x40061526:'206f00044a88',0x400060fe:'4feffff048d70c0c',0x4000614c:'7040d1af0004',
 0x40006158:'4fefffb848d7041c',0x40005f86:'4feffff448d7040c',
 0x4000053a:'720620108081',0x400004d2:'48d000f041e80010'}
LED_SLOTS=[0x400ff9b8,0x400ffa78,0x40103070,0x401031b4,0x40107e18]
def digest(b):return hashlib.sha256(b).hexdigest()
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--stock-syx',type=pathlib.Path,required=True)
 p.add_argument('--output',type=pathlib.Path,default=ROOT/'build/native-stock-reference')
 p.add_argument('--tool-prefix',default='/opt/homebrew/bin/m68k-elf-');a=p.parse_args()
 raw=a.stock_syx.read_bytes();assert digest(raw)==OFFICIAL,'input is not verified official OS1.13'
 sys.path.insert(0,str(ROOT/'reference/Modded-Cycles/tools'))
 from mtlib import aplib,container
 from mtlib.syx import unwrap
 stream,_=unwrap(raw);c=container.parse(stream);sec=next(s for s in c['sections'] if s['id']==3)
 stock=aplib.depack(c['blob'][sec['off']:sec['off']+sec['size']])[0]
 def read(addr,n):return stock[addr-BASE:addr-BASE+n]
 for addr,span,sym in HOOKS:assert read(addr,span).hex()==EXPECTED[addr],f'stock prologue mismatch {addr:08x}'
 for addr in LED_SLOTS:assert read(addr,4)==struct.pack('>I',0x400d7648),f'LED slot mismatch {addr:08x}'
 for addr,val in [(0x400792f2,0x401eb750),(0x400792bc,0x401eb774)]:assert read(addr,4)==struct.pack('>I',val)
 assert read(0x400004ba,6)==bytes.fromhex('207c4019b590')
 assert read(0x400004c0,6)==bytes.fromhex('223c423380b0')
 out=a.output.resolve();out.mkdir(parents=True,exist_ok=True);obj=out/'objects';obj.mkdir(exist_ok=True)
 def run(args,**kw):return subprocess.run(args,check=True,**kw)
 source_hashes={source:digest((ROOT/source).read_bytes()) for source in SOURCES}
 linker_hash=digest((ROOT/'firmware/native.ld').read_bytes())
 objects=[]
 for source in SOURCES:
  target=obj/(source.replace('/','__')+'.o');objects.append(str(target))
  run([a.tool_prefix+'gcc','-mcpu=54415','-Os','-ffreestanding','-fno-builtin','-fno-strict-aliasing',
   '-fno-unwind-tables','-fno-asynchronous-unwind-tables','-fstack-usage','-Wall','-Wextra','-Werror',
   '-c',str(ROOT/source),'-o',str(target)])
 elf=out/'native-stock-reference.elf';payload=out/'native-stock-reference.payload.bin'
 run([a.tool_prefix+'gcc','-mcpu=54415','-nostdlib','-Wl,-T,'+str(ROOT/'firmware/native.ld'),
  '-Wl,-Map,'+str(out/'native-stock-reference.map'),*objects,'-lgcc','-o',str(elf)])
 run([a.tool_prefix+'objcopy','-O','binary',str(elf),str(payload)])
 symbols=subprocess.check_output([a.tool_prefix+'nm','-n',str(elf)],text=True)
 (out/'native-stock-reference.symbols.txt').write_text(symbols)
 sym={line.split()[2]:int(line.split()[0],16) for line in symbols.splitlines() if len(line.split())==3}
 start,end=sym['mc_payload_start'],sym['mc_payload_end'];assert start==PAYLOAD and end%16==0
 blob=payload.read_bytes();assert PAYLOAD+len(blob)<=sym['__bss_start']<=end
 nres=(end-PAYLOAD+BLOCK-1)//BLOCK;assert 1<=nres<=6
 clear_off=sym['mc_clear_hook']-PAYLOAD;clear=blob[clear_off:clear_off+80]
 for op,value in [(b'\xb1\xfc',PAYLOAD),(b'\x20\x7c',end),(b'\xb1\xfc',0x423380b0)]:
  assert op+struct.pack('>I',value) in clear,'compiled BSS clear fails exact skip/end proof'
 assert source_hashes=={source:digest((ROOT/source).read_bytes()) for source in SOURCES},'sources changed during build; rerun'
 assert linker_hash==digest((ROOT/'firmware/native.ld').read_bytes()),'linker script changed during build'
 patches=[]
 def patch(addr,new,kind,symbol=None):
  old=read(addr,len(new));assert len(old)==len(new)
  patches.append({'address':addr,'address_hex':f'0x{addr:08x}','old_hex':old.hex(),'new_hex':new.hex(),
   'kind':kind,**({'target_symbol':symbol,'target_address':sym[symbol]} if symbol else {})})
 for addr,span,name in HOOKS:patch(addr,b'\x4e\xf9'+struct.pack('>I',sym[name])+b'\x4e\x71'*((span-6)//2),'entry_jump',name)
 for addr in LED_SLOTS:patch(addr,struct.pack('>I',sym['mc_led_hook']),'vtable_pointer','mc_led_hook')
 patch(0x400792f2,struct.pack('>I',CACHE_ENTRY+20*nres),'cache_lru_head')
 patch(0x400792bc,struct.pack('>I',CACHE_ENTRY+20*(nres+1)+16),'cache_prev_link_loop')
 image=bytearray(stock);image.extend(b'\0'*(end-BASE-len(image)))
 image[PAYLOAD-BASE:PAYLOAD-BASE+len(blob)]=blob
 for q in patches:
  off=q['address']-BASE;new=bytes.fromhex(q['new_hex']);assert image[off:off+len(new)].hex()==q['old_hex'];image[off:off+len(new)]=new
 assert not any(image[0x401aa140-BASE:PAYLOAD-BASE]),'filesystem chunk-zero buffer contains payload bytes'
 (out/'native-stock-reference.main.bin').write_bytes(image)
 manifest={'status':'stock_reference_only_not_release_not_sysex','hardware_writes':False,
  'installed_configuration':'unknown; no preservation claim','official_syx_sha256':digest(raw),
  'stock_main_sha256':digest(stock),'stock_main_base':BASE,'stock_main_bytes':len(stock),
  'payload_start':start,'payload_file_end':PAYLOAD+len(blob),'bss_start':sym['__bss_start'],
  'payload_memory_end':end,'payload_file_bytes':len(blob),'persistent_bss_bytes':end-sym['__bss_start'],
  'cache_blocks_retired':nres,'cache_reserved_limit':PAYLOAD+nres*BLOCK,'cache_total_blocks':16,
  'cache_metadata_base':CACHE_ENTRY,'cache_block_bytes':BLOCK,'boot_clear_start':0x4019b590,
  'boot_clear_end':0x423380b0,'boot_clear_skip':[start,end],
  'payload_sha256':digest(blob),'reference_main_sha256':digest(image),'patches':patches,
  'sources':source_hashes,'linker_script_sha256':linker_hash,
  'compiler':subprocess.check_output([a.tool_prefix+'gcc','--version'],text=True).splitlines()[0],
  'compile_flags':['-mcpu=54415','-Os','-ffreestanding','-fno-builtin','-fno-strict-aliasing','-fno-unwind-tables','-fno-asynchronous-unwind-tables','-fstack-usage','-Wall','-Wextra','-Werror'],
  'compatibility':'Requires exact installed-modification composition; overlaps Model-TG payload/cache retirement and boot/LED hooks. Syntakt/TG placement not inferred.'}
 (out/'native-stock-reference.manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
 print(json.dumps({k:manifest[k] for k in ['status','payload_file_bytes','persistent_bss_bytes','cache_blocks_retired','payload_start','payload_memory_end']}))
if __name__=='__main__':main()
