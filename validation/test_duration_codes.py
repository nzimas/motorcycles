"""Stock finite duration mapping and actual note-off decrement primitive."""
import ctypes, hashlib, json, pathlib, struct, subprocess
from unicorn import Uc, UC_ARCH_M68K, UC_MODE_BIG_ENDIAN
from unicorn import m68k_const as m
D=pathlib.Path(__file__).resolve().parent.parent
main=(D/'adapter/stock-main.local.bin').read_bytes()
assert hashlib.sha256(main).hexdigest()=='cc99d4f0175d34d1e91d046e6ec85a5e8ab58ab9edbb3c24406acd48cb99ee98'
table=struct.unpack_from('>128I',main,0x4010d814-0x40000400)
subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-shared','-fPIC',str(D/'firmware/duration.c'),'-o',str(D/'build/duration-host.dylib')],check=True)
lib=ctypes.CDLL(str(D/'build/duration-host.dylib'))
lib.mc_duration_codes.argtypes=[ctypes.c_uint,ctypes.POINTER(ctypes.c_uint32),ctypes.POINTER(ctypes.c_uint8)]
rom=(ctypes.c_uint32*128)(*table)
mapping=[]
for speed,ticks in enumerate([3,4,6,8,12,24,48]):
    out=(ctypes.c_uint8*13)(*([255]*13))
    assert lib.mc_duration_codes(speed,rom,out)==1
    expected=[0]+[min(range(127),key=lambda code:(abs(table[code+1]-ticks*450000*duration),code)) for duration in range(1,13)]
    assert list(out)==expected
    mapping.append(expected[1:])
assert mapping[2]==[14,30,38,46,50,54,58,62,64,66,68,70]
for speed in [7,255,0xffffffff]:
    out=(ctypes.c_uint8*13)(*([73]*13))
    assert lib.mc_duration_codes(speed,rom,out)==0 and list(out)==[73]*13
rom[40]=0
out=(ctypes.c_uint8*13)(*([73]*13))
assert lib.mc_duration_codes(2,rom,out)==0 and list(out)==[73]*13
# Actual stock note-off counter path: D1 is current tempo (BPM*120), D2 is
# remaining duration. It subtracts one tempo per audio block at40058cf4.
u=Uc(UC_ARCH_M68K,UC_MODE_BIG_ENDIAN);u.ctl_set_cpu_model(m.UC_CPU_M68K_CFV4E)
u.mem_map(0x40000000,0x200000);u.mem_write(0x40000400,main)
u.mem_map(0x43000000,0x1000)
for tempo in [3600,14400,36000]:
    for code in mapping[2]:
        duration=table[code+1]
        u.reg_write(m.UC_M68K_REG_SR,0x2000)
        u.reg_write(m.UC_M68K_REG_A1,0x43000000)
        u.reg_write(m.UC_M68K_REG_D1,tempo)
        u.reg_write(m.UC_M68K_REG_D2,duration)
        u.emu_start(0x40058cf0,0x40058cf8,count=20)
        assert int.from_bytes(u.mem_read(0x43000000,4),'big')==duration-tempo
report={'result':'PASS','speeds':7,'duration_mappings':84,'actual_stock_note_off_decrements':36,'sixteenth_duration_units':2700000,'triplet_quantization':'nearest stock finite code; maximum allowed relative error 1/16','limitations':'Primitive execution and ROM mapping, not full sequencer scheduling or device playback.'}
(D/'validation/duration-report.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
