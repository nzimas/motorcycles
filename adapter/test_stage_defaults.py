#!/usr/bin/env python3
"""Real target helper and stock defaults; synthetic initialized machine-ID rows.
No device access, no fabricated defaults routine. Actual build must also execute
this test against patched defaults hook for the selected modification manifest."""
exec(open(__file__.replace('test_stage_defaults.py','test_stock_primitives.py')).read().split('# Actual PadEvent')[0])
import subprocess,tempfile
CODE=0x90080000;STAGE=0x90070000
temp=tempfile.TemporaryDirectory();build=pathlib.Path(temp.name);src=pathlib.Path(__file__).parent
prefix='/opt/homebrew/bin/m68k-elf-'
for source in ['firmware.c','fast_blocks.S']:
 subprocess.run([prefix+'gcc','-mcpu=54415','-Os','-ffreestanding','-fno-strict-aliasing','-Wall','-Wextra','-Werror','-c',str(src/source),'-o',str(build/(source+'.o'))],check=True)
subprocess.run([prefix+'ld','-Ttext=0x90080000','-e','mc_fw_stage_machine',str(build/'firmware.c.o'),str(build/'fast_blocks.S.o'),'-o',str(build/'stage.elf')],check=True)
subprocess.run([prefix+'objcopy','-O','binary',str(build/'stage.elf'),str(build/'stage.bin')],check=True)
symbols=subprocess.check_output([prefix+'nm',str(build/'stage.elf')],text=True)
HELPER=int(next(line.split()[0] for line in symbols.splitlines() if line.endswith(' mc_fw_stage_machine')),16)
u.mem_write(CODE,(build/'stage.bin').read_bytes())
# Default page rows mirror initialized descriptor IDs. Stock lookup already
# reads actual immutable descriptor table4010dce0.
for machine in range(6):w(0x40a71540+(machine+1)*76,0x40fe4278,0x40fe4278,*([42,50,46,47,48,49,44,45,43]+[0]*8))
w(OBJ,0x400fef54);w(OBJ+16,DATA,0,0,0,0)
u.mem_write(OBJ+12,b'\0\0');u.mem_write(OBJ+36,b'\0')
for previous in range(6):
 for machine in range(6):
  before=bytearray((i*7+3)%256 for i in range(100));before[38]=previous;before[39]=0
  u.mem_write(DATA,bytes(before));u.mem_write(STAGE,bytes(before))
  try:ret=call(HELPER,OBJ,STAGE,machine)&255
  except Exception:
   print("PC",hex(u.reg_read(R.UC_M68K_REG_PC)),"prev",previous,"machine",machine);raise
  assert ret==1
  assert bytes(u.mem_read(DATA,100))==bytes(before)
  after=bytes(u.mem_read(STAGE,100));assert after[38]==machine
  if machine==previous:assert after==bytes(before)
  else:
   expected=bytearray(before);expected[38]=machine;expected[39]=0
   for descriptor in [50,46,47,48,49]:
    offset=0x4010dce0-BASE+descriptor*56
    slot=struct.unpack('>i',main[offset+4:offset+8])[0]
    default=struct.unpack('>I',main[offset+16:offset+20])[0]
    expected[20+2*slot:22+2*slot]=struct.pack('>H',default)
   assert after==bytes(expected),(previous,machine)
print('PASS36stock transitions same-machine preservation and live100B immutable')
original=bytes(u.mem_read(DATA,100));owner_before=bytes(u.mem_read(OBJ,68))
u.mem_write(STAGE,original)
assert (call(HELPER,OBJ,DATA,0)&255)==0
assert (call(HELPER,OBJ,STAGE,6)&255)==0
u.mem_write(STAGE,b'\x01'+original[1:]);scratch=bytes(u.mem_read(STAGE,100))
assert (call(HELPER,OBJ,STAGE,0)&255)==0
assert bytes(u.mem_read(STAGE,100))==scratch
assert bytes(u.mem_read(DATA,100))==original and bytes(u.mem_read(OBJ,68))==owner_before
print('PASS staged alias/unsupported machine/stale scratch reject without live or scratch writes')
