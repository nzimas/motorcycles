# Model:Cycles R1 independent baseline

The user-supplied file `/Users/nzimas/Downloads/model-cycles_OS1.13/model-cycles_OS1.13.syx` is byte-identical to upstream's official OS 1.13 baseline. Whole-file SHA-256: `44fe586269631a0ca7da25a3383fc6733c314809505fc3cc52f1e0ed9800640c`. Decompressed MAIN OS SHA-256: `cc99d4f0175d34d1e91d046e6ec85a5e8ab58ab9edbb3c24406acd48cb99ee98`; 1,744,192 bytes. Vendor header version is `00381.13`.

Reference upstream commit: `d520df4e3ba1dcd6724548c6ff5705b5bd8ac178`. The local installed-modification configuration remains unidentified; a USB-connected instrument does not identify which optional patches are installed. No installed configuration may be claimed preserved until the user provides it or a separate authorized discovery establishes it.

`baseline.json` records all section hashes and metadata. `validate_package.py` independently checks every SysEx packet checksum, the container content checksum, HMAC, section alignment/bounds/non-overlap, exact transport rewrapping, and pinned official MAIN OS/full-file hashes. With `--candidate`, it additionally requires the internal vendor version and section IDs/attributes to remain unchanged and requires stored sections 5, 2 and 4 to be byte-identical. Relocation of stored sections within the rebuilt container is allowed.

The pinned upstream's overlap proof passes: 84 tweaks, 8,730 writes, 72 compatible appended-payload sets. See `upstream-overlaps.log`. This establishes the upstream catalog's internal declared compatibility, not compatibility of new candidate code or an unknown installed configuration.

## Available tooling and limitations

- `m68k-elf-gcc` GCC 16.2.0 is installed. ColdFire sources use `-mcpu=54418`.
- System Python 3.13 has NumPy 2.3.3 but lacks Unicorn.
- Existing Digitone project's read-only emulation environment has Unicorn 2.1.4 but lacks NumPy. Supplying system site-packages resolves imports.
- Initial sandboxed Unicorn memory mapping exited SIGILL (132), before firmware execution. A separate `validation/venv` now provides system NumPy plus clean Unicorn 2.1.4. Running offline Unicorn proofs outside the sandbox resolves the mapping restriction. The pinned upstream trig-hold input-dispatch proof and boot-animation subsystem proof both pass (exit 0); logs are retained. These upstream proofs do not test new generator integration.
- Upstream emulators execute selected real OS code with synthetic objects and intercepted peripheral/project calls. The sound-engine rig performs ROM-to-SRAM copy and voice initialization; the animation rig runs the animation task. These are not a full instrument boot/peripheral emulator. Passing subsystem proofs alone does not establish complete boot, actual playback update safety, USB upgrade availability, or physical-device behavior.

No physical instrument access, MIDI traffic or deployment was performed by this validator.
