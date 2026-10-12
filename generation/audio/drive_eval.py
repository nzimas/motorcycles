#!/usr/bin/env python3
"""Target-cost and bit-exactness check for firmware/drive.c.

Compiles drive.c for ColdFire (m68k-elf-gcc -mcpu=54415) and for the host
(shared library), runs mc_drive_block under Unicorn (CFV4E) with the sample
block placed at the real fast-SRAM address 0x80001858, counts executed
instructions per 32-sample block and compares every output word with the host
build.  Usage: validation/venv/bin/python generation/audio/drive_eval.py [-O2|-Os]
"""
import ctypes, json, pathlib, struct, subprocess, sys, tempfile
import numpy as np
from unicorn import Uc, UC_ARCH_M68K, UC_MODE_BIG_ENDIAN, UC_HOOK_CODE
from unicorn import m68k_const as m

ROOT = pathlib.Path(__file__).resolve().parents[2]
SRC = ROOT / 'firmware' / 'drive.c'
GCC, NM, SIZE = '/opt/homebrew/bin/m68k-elf-gcc', '/opt/homebrew/bin/m68k-elf-nm', '/opt/homebrew/bin/m68k-elf-size'
OPT = sys.argv[1] if len(sys.argv) > 1 else '-O2'
EXTRA = sys.argv[2:]  # e.g. -DMC_DRIVE_UNROLL=2
CODE, SRAM, BUF, PRM, ST, STACK, STOP = 0x10000, 0x80000000, 0x80001858, 0x80002000, 0x80002100, 0x44008000, 0x45000000
NB = 320
FLAGS = ['-mcpu=54415', OPT, '-ffreestanding', '-fno-builtin', '-Wall', '-Wextra', '-Werror', *EXTRA]


def build(tmp):
    elf = tmp / 'drive.elf'
    subprocess.run([GCC, *FLAGS, '-nostdlib', '-Wl,-Ttext=0x10000', '-Wl,-e,mc_drive_block', str(SRC), '-o', str(elf)], check=True)
    subprocess.run(['/opt/homebrew/bin/m68k-elf-objcopy', '-O', 'binary', str(elf), str(tmp / 'drive.bin')], check=True)
    obj = tmp / 'drive.o'
    subprocess.run([GCC, *FLAGS, '-c', str(SRC), '-o', str(obj)], check=True)
    text = int(subprocess.run([SIZE, str(obj)], capture_output=True, text=True).stdout.split('\n')[1].split()[0])
    syms = {l.split()[2]: int(l.split()[0], 16) for l in subprocess.run([NM, str(elf)], capture_output=True, text=True).stdout.splitlines() if len(l.split()) == 3}
    so = tmp / 'drive_host.so'
    subprocess.run(['cc', '-std=c11', '-O1', '-shared', '-fPIC', *EXTRA, str(SRC), '-o', str(so)], check=True)
    return (tmp / 'drive.bin').read_bytes(), syms, text, ctypes.CDLL(str(so))


class Target:
    def __init__(self, blob, syms):
        self.u = Uc(UC_ARCH_M68K, UC_MODE_BIG_ENDIAN)
        self.u.ctl_set_cpu_model(m.UC_CPU_M68K_CFV4E)
        self.u.mem_map(CODE, 0x10000); self.u.mem_map(SRAM, 0x10000)
        self.u.mem_map(STACK - 0x8000, 0x10000); self.u.mem_map(STOP, 0x1000)
        self.u.mem_write(CODE, blob)
        self.syms, self.n = syms, 0
        self.u.hook_add(UC_HOOK_CODE, self._count)
        self.u.mem_write(ST, bytes(256))

    def _count(self, u, a, s, d):
        self.n += 1

    def call(self, name, *args):
        self.u.mem_write(STACK, struct.pack('>I' + 'I' * len(args), STOP, *args))
        self.u.reg_write(m.UC_M68K_REG_SR, 0x2700)
        self.u.reg_write(m.UC_M68K_REG_A7, STACK)
        self.n = 0
        self.u.emu_start(self.syms[name], STOP, count=200000)
        assert self.u.reg_read(m.UC_M68K_REG_PC) == STOP
        for r in (m.UC_M68K_REG_A7,):
            assert self.u.reg_read(r) == STACK + 4
        return self.n - 0

    def block(self, x, params):
        self.u.mem_write(BUF, struct.pack('>32i', *[int(v) for v in x]))
        self.u.mem_write(PRM, bytes(params))
        n = self.call('mc_drive_block', BUF, PRM, ST)
        return np.array(struct.unpack('>32i', bytes(self.u.mem_read(BUF, 128))), dtype=np.int64), n


def host_block(lib, st, x, params):
    arr = (ctypes.c_int32 * 32)(*[int(v) for v in x])
    lib.mc_drive_block(arr, (ctypes.c_uint8 * 8)(*params), st)
    return np.array(arr[:], dtype=np.int64)


def signal(kind, n, rng):
    t = np.arange(n) / 48000.0
    if kind == 'sine_hot':
        return 0.9 * np.sin(2 * np.pi * 110 * t)
    if kind == 'kick':
        return 0.95 * np.exp(-t * 6) * np.sin(2 * np.pi * (50 + 120 * np.exp(-t * 40)) * t)
    if kind == 'noise':
        return np.clip(rng.normal(0, 0.3, n), -1, 1)
    return 0.3 * np.sin(2 * np.pi * 440 * t)


def main():
    rng = np.random.default_rng(1)
    with tempfile.TemporaryDirectory() as td:
        blob, syms, text, lib = build(pathlib.Path(td))
        cases = {
            'bypass (amount 0)': [0, 90, 90, 90, 90, 90, 0, 0],
            'typical (amount 64)': [64, 0, 0, 0, 0, 0, 0, 0],
            'drive+tone (64, drive 20, tone 40)': [64, 20, 0, 0, 40, 0, 0, 0],
            'drive+tone+low (64, 20, tone 40, low 30)': [64, 20, 0, 0, 40, 30, 0, 0],
            'fold only (100, fold 90)': [100, 0, 90, 0, 0, 0, 0, 0],
            'bias only (80, bias 60)': [80, 0, 0, 60, 0, 0, 0, 0],
            'worst (all features)': [127, 127, 127, 127, 127, 127, 0, 0],
        }
        report = {'opt': OPT, 'text_bytes': text, 'cases': {}}
        mismatches = 0
        for name, prm in cases.items():
            for kind in ('sine_hot', 'kick', 'noise', 'quiet'):
                tgt = Target(blob, syms)
                st = ctypes.create_string_buffer(256)
                sig = np.round(signal(kind, 32 * NB, rng) * 2147483647).astype(np.int64)
                counts = []
                for b in range(NB):
                    x = sig[32 * b:32 * b + 32]
                    yt, n = tgt.block(x, prm)
                    yh = host_block(lib, st, x, prm)
                    mismatches += int(np.sum(yt != yh))
                    counts.append(n)
                c = report['cases'].setdefault(name, {'steady': 0, 'glide': 0})
                c['steady'] = max(c['steady'], max(counts[-20:])); c['glide'] = max(c['glide'], max(counts[1:]))
        # fuzz: random parameter records (changing every 1..8 blocks, any byte
        # value) and random/extreme inputs, target vs host on every word
        fuzz_blocks, max_n, max_sp = 0, 0, 0
        for seed in range(6):
            r = np.random.default_rng(100 + seed)
            tgt = Target(blob, syms)
            sp_min = [STACK]
            tgt.u.hook_add(UC_HOOK_CODE, lambda u, a, s_, d: sp_min.__setitem__(0, min(sp_min[0], u.reg_read(m.UC_M68K_REG_A7))))
            st = ctypes.create_string_buffer(256)
            prm = [0] * 8
            for b in range(400):
                if b % int(r.integers(1, 9)) == 0:
                    prm = [int(v) for v in r.integers(0, 256, 8)]
                    if r.random() < 0.5:
                        prm = [min(v, 127) for v in prm]
                    if r.random() < 0.15:
                        prm[0] = 0
                kind = (b // 50 + seed) % 4
                if kind == 0:
                    x = r.integers(-2**31, 2**31, 32)
                elif kind == 1:
                    x = np.where(r.random(32) < 0.5, -2**31, 2**31 - 1)
                elif kind == 2:
                    x = np.round(0.9 * np.sin(np.arange(32) * 0.05 + b) * 2**31)
                else:
                    x = r.integers(-3, 4, 32)
                yt, n = tgt.block(x, prm)
                yh = host_block(lib, st, x, prm)
                mismatches += int(np.sum(yt != yh))
                max_n = max(max_n, n)
                fuzz_blocks += 1
            max_sp = max(max_sp, STACK - sp_min[0])
        report['fuzz_blocks'] = fuzz_blocks
        report['fuzz_max_block_instr'] = max_n
        report['max_stack_bytes'] = max_sp
        # parameter-change block cost (derive() runs): worst case of a fresh activation
        tgt = Target(blob, syms)
        _, n_first = tgt.block([0] * 32, [127, 127, 127, 127, 127, 127, 0, 0])
        _, n_change = tgt.block([0] * 32, [126, 127, 127, 127, 127, 127, 0, 0])
        report['activation_block_instr'] = n_first
        report['param_change_block_instr'] = n_change
        report['bit_mismatches_vs_host'] = mismatches
        print(f"{'case':42s} steady instr/block (/sample)   while gliding (/sample)")
        for name, c in report['cases'].items():
            print(f"{name:42s} {c['steady']:5d} ({c['steady']/32:5.2f})          {c['glide']:5d} ({c['glide']/32:5.2f})")
        print(f"fuzz: {fuzz_blocks} blocks compared, max {max_n} instr in one block, max stack depth {max_sp} bytes")
        print(f"activation block {n_first} instr, parameter-change block {n_change} instr, .text {text} bytes, {OPT} {' '.join(EXTRA)}")
        print(('PASS' if mismatches == 0 else 'FAIL') + f' target==host bit-exact ({mismatches} mismatching words)')
        print(json.dumps(report))
        return 0 if mismatches == 0 else 1


if __name__ == '__main__':
    sys.exit(main())
