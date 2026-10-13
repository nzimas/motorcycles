#!/usr/bin/env python3
"""Build selected-mod integration candidate offline; no device access."""
import argparse,hashlib,json,pathlib,struct,subprocess,sys
ROOT=pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'firmware'))
from importlib.machinery import SourceFileLoader
native=SourceFileLoader('native_builder',str(ROOT/'firmware/build-native.py')).load_module()
sys.path.insert(0,str(ROOT/'reference/Modded-Cycles/tools'))
from mtlib import aplib,container
from mtlib.syx import unwrap,wrap,BYTES_PER_MSG
import aplib_grow
BASE=0x40000400;RUNTIME=0x43060000
# RAM above RC17's end (0x4307D8E0) hangs the unit at boot (RC18 and diagnostic D,
# RC17 padded by 5.4 KB, both hung on hardware). Never grow past it.
RUNTIME_LIMIT=0x4307d8e0
# The image (packed payload + trampoline after the OS) must not end past RC17's
# 0x401CEFCC either: RC18 and diagnostic D/E grew it and hung at boot.
IMAGE_LIMIT=0x401cefcc
# Per-track distortion stage (firmware/drive*.c): audio hook and sound-record wrappers.
DRIVE_SOURCES=['firmware/drive.c','firmware/djfilter.c','firmware/drive_ui.c','firmware/drive_hooks.S','firmware/ctrlall.c','firmware/ctrlall_hooks.S','firmware/lyra.c','firmware/brk.c','firmware/brk_hooks.S']
DRIVE_ENTRIES=[(0x4005afa0,'mc_sound_save2_hook'),(0x4005b054,'mc_sound_save1_hook'),(0x4005aecc,'mc_sound_load_hook')]
# LYRA replaces CHORD (machine 5, firmware/lyra.c). The MACRO tweak keeps the
# voice-loop tables, names and descriptors in its boot-copied payload, stored in
# the image at file offsets (VA = BASE + offset before the copy):
# update/render entry 5, the "Chord" string, descriptors 71..74 (COLOR, SHAPE,
# SWEEP, CONTOUR: long name +0x2c, short name +0x34, max +0x0c, default +0x10),
# CHORD SHAPE's knob graphic scale (37) and detent list, and the old-file
# SHAPE conversion for machine 5.
LYRA_UPDATE,LYRA_RENDER=BASE+0x1c1f88,BASE+0x1c1fa4
LYRA_DESCRIPTORS=[(BASE+0x1c2f90,'spread',0x1400),(BASE+0x1c2fc8,'sharp',0x2800),(BASE+0x1c3000,'mod',0x1400),(BASE+0x1c3038,'hyper',0x0000)]
# Latched Control All (firmware/ctrlall.c): stock encoder handlers' held-key
# function register (lea 0x4007faf4,a4 / ,a2), both calls of the lock applier
# 0x400583da, and the knob-edit handler entry (move.l a2,-(sp); move.l d2,-(sp); moveq #6,d2).
CTRL_ALL_SITES=[(0x4001f55c,'49f94007faf4'),(0x4001aaa8,'45f94007faf4'),(0x40058f36,'4eb9400583da'),(0x40059194,'4eb9400583da'),(0x40059534,'2f0a2f027406')]
sha=lambda b:hashlib.sha256(b).hexdigest()
def main():
 p=argparse.ArgumentParser(description=__doc__);g=p.add_mutually_exclusive_group(required=True);g.add_argument('--stock',type=pathlib.Path,help='official OS1.13; selected mods are rebuilt from it');g.add_argument('--base',type=pathlib.Path,help='existing selected-mods modded firmware (pinned baseline package)');p.add_argument('--output',type=pathlib.Path,default=ROOT/'build/selected-native');a=p.parse_args()
 if a.stock:assert sha(a.stock.read_bytes())==native.OFFICIAL
 else:assert sha(a.base.read_bytes())==json.loads((ROOT/'firmware/selected-configuration.json').read_text())['baseline_package_sha256'],'modded base package differs from pinned selected baseline'
 config=json.loads((ROOT/'firmware/selected-configuration.json').read_text());out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
 prefix='/opt/homebrew/bin/m68k-elf-'
 run=lambda args:subprocess.run([str(x) for x in args],check=True)
 baseline=out/'selected-baseline.local.syx'
 if a.stock:run([sys.executable,ROOT/'reference/Modded-Cycles/tools/build.py','-i',a.stock,'-t',','.join(config['upstream_tweak_ids']),'-o',baseline])
 elif a.base.resolve()!=baseline:baseline.write_bytes(a.base.read_bytes())
 c=container.parse(unwrap(baseline.read_bytes())[0]);s=next(s for s in c['sections'] if s['id']==3)
 before=aplib.depack(c['blob'][s['off']:s['off']+s['size']])[0]
 assert sha(before)==config['baseline_main_sha256'],'pinned selected baseline differs'
 rd=lambda addr,n:before[addr-BASE:addr-BASE+n]
 hooks=[h for h in native.HOOKS if h[2]!='mc_clear_hook']
 for addr,n,name in hooks:assert rd(addr,n).hex()==native.EXPECTED[addr]
 # No Model-TG: LED callbacks are the stock no-op, chained by mc_led_hook.
 for addr in native.LED_SLOTS:assert rd(addr,4)==struct.pack('>I',0x400d7648),'selected LED slot mismatch'
 # Standalone MACRO: the stock BSS clear starts with jmp to its stub, which
 # clears and fills 0x43000000.. and resumes the clear at 0x400004ba.
 assert rd(0x400004b2,8)==bytes.fromhex('4ef94016cae84e71'),'MACRO boot stub entry mismatch'
 assert rd(0x4016cae8,8)==bytes.fromhex('43f943000000203c'),'MACRO stub mismatch'
 macro_end=0x43000000+int.from_bytes(rd(0x4016caf0,4),'big')*4
 assert 0x43000000<macro_end<=RUNTIME,'MACRO runtime overlaps native'
 # Distortion stage sites: the stock Volume+Dist call and the sound save/load entries.
 assert rd(0x40059852,6)==bytes.fromhex('4eb940056f58'),'Volume+Dist call mismatch'
 for addr,_ in DRIVE_ENTRIES:assert rd(addr,8)==bytes.fromhex('4feffff448d71c00'),f'sound record entry mismatch {addr:08x}'
 # LYRA sites.
 assert rd(LYRA_UPDATE,4).hex()=='400aae88' and rd(LYRA_RENDER,4).hex()=='400ab24c','CHORD engine entries mismatch'
 assert rd(0x401300ee,6)==b'Chord\0','CHORD name mismatch'
 assert [rd(a,4).hex() for a,_,_ in LYRA_DESCRIPTORS]==['00000005']*4,'CHORD descriptors mismatch'
 assert rd(LYRA_DESCRIPTORS[1][0]+0x0c,4).hex()=='00002500','CHORD SHAPE max mismatch'
 assert rd(0x40046eb4,4).hex()=='48782500' and rd(0x4004fd00,2).hex()=='720b' and rd(0x4004fd04,6).hex()=='203c4010a09c','CHORD SHAPE UI mismatch'
 assert rd(0x4005cc46,2).hex()=='6610','old CHORD conversion mismatch'
 # Live break: trig builder entry and success exit.
 assert rd(0x400548ce,6).hex()=='4fefffc47201' and rd(0x40054c0e,6).hex()=='254e0048200a','trig builder mismatch'
 # Control All sites: held-key query register loads, lock applier calls, knob-edit handler entry.
 for addr,old in CTRL_ALL_SITES:assert rd(addr,len(bytes.fromhex(old))).hex()==old,f'Control All site mismatch {addr:08x}'
 # Boot call to the BSS clear: jsr 0x400004b2 then lea 0xfc044000,a0.
 assert rd(0x40000530,10)==bytes.fromhex('4ebaff8041f9fc044000'),'boot clear call mismatch'
 ld=(ROOT/'firmware/native.ld').read_text().replace('0x401ab750',hex(RUNTIME)).replace('0x401c3750',hex(RUNTIME_LIMIT))
 linker=out/'native-selected.ld';linker.write_text(ld)
 sources=list(native.SOURCES)+DRIVE_SOURCES;obj=out/'objects';obj.mkdir(exist_ok=True);objects=[]
 hashes={s:sha((ROOT/s).read_bytes()) for s in sources}
 for src in sources:
  target=obj/(src.replace('/','__')+'.o');objects.append(target)
  run([prefix+'gcc','-DMC_SELECTED_MODS=1','-mcpu=54415','-O2' if src in ('firmware/drive.c','firmware/djfilter.c') else '-Os','-ffreestanding','-fno-builtin','-fno-strict-aliasing','-fno-unwind-tables','-fno-asynchronous-unwind-tables','-fstack-usage',*([] if src=='generation/runtime.c' or __import__('os').environ.get('MC_NO_LTO') else ['-flto']),*(['-ffunction-sections','-fdata-sections'] if __import__('os').environ.get('MC_GC') else []),'-Wall','-Wextra','-Werror','-c',ROOT/src,'-o',target])
 stem=out/'ModelCycles-generative-selected';elf=stem.with_suffix('.elf');payload=stem.with_suffix('.payload.bin')
 # Entry points reached only through patched stock bytes must survive LTO.
 keep=set(__import__('re').findall(r"sym\['([A-Za-z_0-9]+)'\]",(ROOT/'firmware/build-selected.py').read_text()))|{h[2] for h in hooks}|{n for _,n in DRIVE_ENTRIES}|{'lyra_name_'+n for _,n,_ in LYRA_DESCRIPTORS}|{'lyra_short_'+n for _,n,_ in LYRA_DESCRIPTORS}|{'mc_led_hook','mc_boot_continue','mc_startup','mc_clock'}
 # ...and the entry points the emulator tests (validation/*.py) call by name.
 keep|={'mc_generate','mc_commit_live','mc_retune_live','mc_binding_resolve','mc_fw_stage_machine','mc_fw_copy_blocks','mc_fw_equal_blocks','mc_music_read','mc_music_write','mc_read_machines','mc_native_init','mc_now','mc_led_mode','mc_ca_tick','mc_native','mc_brk','mc_ca_on','mc_music_context_stamp'}
 run([prefix+'gcc','-mcpu=54415',*([] if __import__('os').environ.get('MC_NO_LTO') else ['-flto']),*(['-Wl,--gc-sections'] if __import__('os').environ.get('MC_GC') else []),'-Os','-ffreestanding','-fno-builtin','-fno-strict-aliasing','-nostdlib',*['-Wl,--undefined='+k for k in sorted(keep)],'-Wl,-T,'+str(linker),'-Wl,-Map,'+str(stem.with_suffix('.map')),*objects,'-lgcc','-o',elf]);run([prefix+'objcopy','-O','binary',elf,payload])
 symbols=subprocess.check_output([prefix+'nm','-n',str(elf)],text=True);stem.with_suffix('.symbols.txt').write_text(symbols)
 sym={l.split()[2]:int(l.split()[0],16) for l in symbols.splitlines() if len(l.split())==3};blob=payload.read_bytes();assert sym['mc_payload_start']==RUNTIME and sym['mc_payload_end']<=RUNTIME_LIMIT
 # The packed copy sits after the image and is inside the BSS the boot clears,
 # so copy it to RUNTIME first, then run the original BSS clear (which also
 # chains MACRO's copier), replay the overwritten lea and return past it.
 source=(BASE+len(before)+15)&~15;copy_size=(len(blob)+3)&~3;trampoline=source+copy_size
 assembly=out/'boot-copy.S';assembly.write_text(f''' .text\n .globl mc_selected_copy\nmc_selected_copy:\n lea -16(%sp),%sp\n movem.l %d0-%d1/%a0-%a1,(%sp)\n movea.l #0x{source:x},%a0\n movea.l #0x{RUNTIME:x},%a1\n move.l #{copy_size//4},%d0\n1: move.l (%a0)+,(%a1)+\n subq.l #1,%d0\n bne 1b\n movem.l (%sp),%d0-%d1/%a0-%a1\n lea 16(%sp),%sp\n jmp 0x{sym['mc_boot_continue']:x}\n''')
 run([prefix+'as','-mcpu=54415',assembly,'-o',out/'boot-copy.o']);run([prefix+'ld','-Ttext='+hex(trampoline),out/'boot-copy.o','-o',out/'boot-copy.elf']);run([prefix+'objcopy','-O','binary',out/'boot-copy.elf',out/'boot-copy.bin'])
 copy=(out/'boot-copy.bin').read_bytes();image=bytearray(before);image.extend(b'\0'*(source-BASE-len(image)));image.extend(blob+b'\0'*(copy_size-len(blob))+copy);assert BASE+len(image)<=IMAGE_LIMIT,f'image ends at {BASE+len(image):#x}, past the proven {IMAGE_LIMIT:#x}'
 patches=[]
 def patch(addr,data,kind):
  old=rd(addr,len(data));assert len(old)==len(data);patches.append(dict(address=addr,old_hex=old.hex(),new_hex=data.hex(),kind=kind));image[addr-BASE:addr-BASE+len(data)]=data
 for addr,span,name in hooks:patch(addr,b'\x4e\xf9'+struct.pack('>I',sym[name])+b'\x4e\x71'*((span-6)//2),'entry_jump')
 for addr in native.LED_SLOTS:patch(addr,struct.pack('>I',sym['mc_led_hook']),'chained_led')
 patch(0x40000530,b'\x4e\xb9'+struct.pack('>I',trampoline),'boot_copy_trampoline')
 patch(0x40059852,b'\x4e\xb9'+struct.pack('>I',sym['mc_drive_audio_hook']),'drive_audio_call')
 for addr,name in DRIVE_ENTRIES:patch(addr,b'\x4e\xf9'+struct.pack('>I',sym[name])+b'\x4e\x71','sound_record_wrapper')
 patch(LYRA_UPDATE,struct.pack('>I',sym['lyra_update']),'lyra_engine')
 patch(LYRA_RENDER,struct.pack('>I',sym['lyra_render']),'lyra_engine')
 patch(0x401300ee,b'Lyra\0\0','lyra_name')
 for addr,name,default in LYRA_DESCRIPTORS:
  patch(addr+0x2c,struct.pack('>I',sym['lyra_name_'+name]),'lyra_descriptor')
  patch(addr+0x34,struct.pack('>I',sym['lyra_short_'+name]),'lyra_descriptor')
  patch(addr+0x10,struct.pack('>I',default),'lyra_descriptor')
 patch(LYRA_DESCRIPTORS[1][0]+0x0c,struct.pack('>I',0x7f00),'lyra_descriptor')
 patch(0x40046eb4,bytes.fromhex('48787f00'),'lyra_shape_ui')
 patch(0x4004fd00,bytes.fromhex('7204'),'lyra_shape_ui')
 patch(0x4004fd04,bytes.fromhex('203c4010a120'),'lyra_shape_ui')
 patch(0x4005cc46,bytes.fromhex('6010'),'lyra_no_chord_conversion')
 # Diagnostic switch: MC_NO_BREAK_HOOKS=1 links the live-break code but leaves the trig builder stock.
 if not __import__('os').environ.get('MC_NO_BREAK_HOOKS'):
  patch(0x400548ce,b'\x4e\xb9'+struct.pack('>I',sym['mc_brk_entry_hook']),'break_step')
  patch(0x40054c0e,b'\x4e\xb9'+struct.pack('>I',sym['mc_brk_exit_hook']),'break_message')
 patch(0x4001f55c,b'\x49\xf9'+struct.pack('>I',sym['mc_held']),'control_all_held')
 patch(0x4001aaa8,b'\x45\xf9'+struct.pack('>I',sym['mc_held']),'control_all_held')
 for addr in (0x40058f36,0x40059194):patch(addr,b'\x4e\xb9'+struct.pack('>I',sym['mc_ca_locks']),'control_all_locks')
 patch(0x40059534,b'\x4e\xf9'+struct.pack('>I',sym['mc_param_edit_hook']),'control_all_param_edit')
 assert hashes=={s:sha((ROOT/s).read_bytes()) for s in sources}
 stem.with_suffix('.main.bin').write_bytes(image)
 # Repack from the official package, or from the modded base package (whose
 # protected sections are the official ones). Only MAIN section3 changes.
 stock_raw=(a.stock or a.base).read_bytes();stream,transport=unwrap(stock_raw);stock=container.parse(stream);sec=next(s for s in stock['sections'] if s['id']==3);old,ops=aplib.depack(stock['blob'][sec['off']:sec['off']+sec['size']]);dirty=bytearray(a!=b for a,b in zip(old,image));dirty.extend(b'\1'*(len(image)-len(old)))
 packed=aplib_grow.repack_grow(bytes(image),ops,dirty,len(old));assert aplib.depack(packed)[0]==bytes(image)
 plains=[]
 for sec in stock['sections']:
  try:plains.append(aplib.depack(stock['blob'][sec['off']:sec['off']+sec['size']])[0])
  except Exception:pass
 key=container.find_key(plains,stock['blob'][:-32],stock['blob'][-32:]);assert key
 rebuilt=container.rebuild(stock,{3:packed},key);output=wrap(container.build_stream(rebuilt,BYTES_PER_MSG),transport['product'],transport['start_seq']);stem.with_suffix('.syx').write_bytes(output)
 manifest=dict(status='offline_candidate_requires_validation',hardware_transfer=False,selected_configuration=config,sources=hashes,patches=patches,baseline_main_sha256=sha(before),main_sha256=sha(image),package_sha256=sha(output),payload_sha256=sha(blob),payload_start=RUNTIME,payload_memory_end=sym['mc_payload_end'],bss_start=sym['__bss_start'],payload_file_bytes=len(blob),source_start=source,source_end=source+copy_size,trampoline_start=trampoline,trampoline_bytes=len(copy),boot_chain=[0x40000530,trampoline,sym['mc_boot_continue'],0x400004b2,0x4016cae8],led_chain=0x400d7648,macro_runtime_end=macro_end,runtime_limit=RUNTIME_LIMIT,main_end=BASE+len(image))
 stem.with_suffix('.manifest.json').write_text(json.dumps(manifest,indent=2)+'\n');print(json.dumps({k:manifest[k] for k in ['status','package_sha256','payload_start','payload_memory_end','main_end']}))
if __name__=='__main__':main()
