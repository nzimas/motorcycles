import pathlib,json,hashlib,argparse
p=argparse.ArgumentParser();p.add_argument("--live-shift",type=int,default=0);p.add_argument("--stage-shift",type=int,default=0);p.add_argument("--out",default="transaction-target-report.json");args=p.parse_args()
from unicorn import Uc,UC_ARCH_M68K,UC_MODE_BIG_ENDIAN,UC_HOOK_CODE
from unicorn import m68k_const as m
D=pathlib.Path(__file__).resolve().parent
symbols={l.split()[2]:int(l.split()[0],16) for l in (D/'transaction-target-symbols.txt').read_text().splitlines()}
u=Uc(UC_ARCH_M68K,UC_MODE_BIG_ENDIAN);u.ctl_set_cpu_model(m.UC_CPU_M68K_CFV4E)
u.mem_map(0x43000000,0x10000);u.mem_map(0x43100000,0x40000);u.mem_map(0x44000000,0x10000);u.mem_map(0x45000000,0x1000)
u.mem_write(0x43000000,(D/'transaction-target.bin').read_bytes());sp=0x4400fff0;stop=0x45000000
u.reg_write(m.UC_M68K_REG_SR,0x2700);u.reg_write(m.UC_M68K_REG_A7,sp);u.mem_write(sp,stop.to_bytes(4,'big'))
count=critical_count=0;critical=False;low=sp

def code(u,a,n,d):
 global count,critical_count,critical,low
 count+=1;low=min(low,u.reg_read(m.UC_M68K_REG_A7))
 if a==symbols['test_irq_save']:critical=True
 if a==symbols['test_irq_restore']:critical=False
 if critical:critical_count+=1
u.hook_add(UC_HOOK_CODE,code);u.emu_start(symbols['run_transaction'],stop,count=10000000)
assert u.reg_read(m.UC_M68K_REG_PC)==stop
assert int.from_bytes(u.mem_read(symbols['result'],4),'big')==1
assert bytes(u.mem_read(symbols['live']+args.live_shift,30710))==b'\xaa'*30710
assert bytes(u.mem_read(symbols['live']+30712,600))==b'\xaa'*600
if args.live_shift==0:assert bytes(u.mem_read(symbols['live']+30710,2))==b'\x55'*2
else:assert bytes(u.mem_read(symbols['live'],args.live_shift))==b'\x55'*args.live_shift
assert bytes(u.mem_read(symbols['live']+31312,32688))==b'\x55'*32688
assert int.from_bytes(u.mem_read(symbols['notifications'],4),'big')==1
r={'result':'PASS','transaction_bytes':31310,'live_shift':args.live_shift,'stage_shift':args.stage_shift,'total_instructions':count,'critical_section_instructions':critical_count,'max_observed_stack':sp-low,'illustrative_one_cycle_per_instruction_ms_at_135168000hz':critical_count/135168,'elf_sha256':hashlib.sha256((D/'transaction-target.elf').read_bytes()).hexdigest(),'limitations':'Standalone transaction core, synthetic IRQ callbacks, instruction count not cycle-accurate hardware latency. Instruction count does not prove ISR deadline compliance; no formal hardware cycle lower bound claimed.'}
(D/args.out).write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r,indent=2))
