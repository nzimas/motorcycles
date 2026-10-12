"""Execute compiled wrappers; handler stubs test ABI, not firmware publication."""
import pathlib, json
from unicorn import Uc, UC_ARCH_M68K, UC_MODE_BIG_ENDIAN
from unicorn import m68k_const as m
D = pathlib.Path(__file__).resolve().parent.parent
symbols = {s[2]: int(s[0], 16) for line in (D/'build/hooks-proof-symbols.txt').read_text().splitlines() if len(s := line.split()) == 3}
blob = (D/'build/hooks-proof.bin').read_bytes()
def vm():
    u = Uc(UC_ARCH_M68K, UC_MODE_BIG_ENDIAN)
    u.ctl_set_cpu_model(m.UC_CPU_M68K_CFV4E)
    for base, size in [(0x43000000, 0x20000), (0x43100000, 0x1000), (0x44000000, 0x10000), (0x45000000, 0x1000), (0x40000000, 0x10000)]:
        u.mem_map(base, size)
    u.mem_write(0x43000000, blob)
    u.reg_write(m.UC_M68K_REG_SR, 0x2000)
    u.reg_write(m.UC_M68K_REG_A7, 0x4400ff00)
    return u
checks = 0
for hook, continuation, frame in [('mc_key_hook',0x40006106,16), ('mc_pad_hook',0x40006152,0), ('mc_encoder_hook',0x40006160,72)]:
    for consumed in [0,1]:
        u=vm(); sp=0x4400ff00; stop=0x45000000
        u.mem_write(sp, b''.join(x.to_bytes(4,'big') for x in [stop,0x12340000,0x12350000]))
        u.mem_write(0x43100008, consumed.to_bytes(4,'big'))
        regs=[m.UC_M68K_REG_D0,m.UC_M68K_REG_D1,m.UC_M68K_REG_A0,m.UC_M68K_REG_A1]
        for i,r in enumerate(regs): u.reg_write(r,0x100+i)
        u.emu_start(symbols[hook],stop if consumed else continuation,count=1000)
        assert bytes(u.mem_read(0x43100000,8)) == bytes.fromhex('1234000012350000')
        assert u.reg_read(m.UC_M68K_REG_PC) == (stop if consumed else continuation)
        assert u.reg_read(m.UC_M68K_REG_A7) == (sp+4 if consumed else sp-frame)
        for i,r in enumerate(regs):
            expected=1 if consumed and i==0 else (64 if hook=='mc_pad_hook' and not consumed and i==0 else 0x100+i)
            assert u.reg_read(r)==expected
        if hook=='mc_pad_hook' and not consumed:
            assert int.from_bytes(u.mem_read(sp+4,4),'big')==0x12340040
        checks+=1
u=vm(); sp=0x4400ff00
u.mem_write(sp,b''.join(x.to_bytes(4,'big') for x in [0x45000000,0x11110000,0x22220000,7]))
u.emu_start(symbols['mc_light_hook'],0x40005f8e,count=1000)
assert bytes(u.mem_read(0x43100000,8))==bytes.fromhex('2222000000000007')
assert int.from_bytes(u.mem_read(sp+12,4),'big')==2
assert u.reg_read(m.UC_M68K_REG_A7)==sp-12
checks+=1
u=vm(); sp=0x4400ff00
u.mem_write(sp,(0x45000000).to_bytes(4,'big'))
u.reg_write(m.UC_M68K_REG_SR,0x2300)
u.emu_start(symbols['mc_irq_save'],0x45000000,count=100)
assert u.reg_read(m.UC_M68K_REG_D0)==0x2300
assert u.reg_read(m.UC_M68K_REG_SR)==0x2700
u.reg_write(m.UC_M68K_REG_A7,sp)
u.mem_write(sp+4,(0x2300).to_bytes(4,'big'))
u.emu_start(symbols['mc_irq_restore'],0x45000000,count=100)
assert u.reg_read(m.UC_M68K_REG_SR)==0x2300
checks+=1
print(json.dumps({'result':'PASS','checks':checks,'limitations':'Standalone wrappers with synthetic C handlers. No final package, boot, device, or audio-publication proof.'},indent=2))
