# ZX Spectrum Bare Metal for Raspberry Pi 3B+

This project combines the [ZOT ZX Spectrum](https://github.com/antirez/ZOT) emulator core with a Raspberry Pi bare-metal frontend built on the [Circle](https://github.com/rsta2/circle) environment.

The active frontend is `circle-zx/`.

## Features

- ZX Spectrum 48K core with native 128K snapshot paging
- Scaled framebuffer output
- USB keyboard input mapping
- Kempston joystick mapping (keyboard)
- OSD browser for `.z80`, `.tap`, `.tzx`
- OSD keyboard typing reference (readable text)
- Tape playback controls and turbo tape mode
- PWM beeper audio output

## Build (Raspberry Pi 3B+)

1. Build Circle libraries:

```bash
cd circle
./makeall clean RASPPI=3
./makeall RASPPI=3
cd addon/SDCard && make RASPPI=3
```

2. Build kernel image:

```bash
cd /path/to/zx-pi-metal
make circle_zx CIRCLEHOME=/path/to/zx-pi-metal/circle RASPPI=3 AARCH=32
```

Generated image:

- `circle-zx/kernel8-32.img`

## SD Card Boot Files

Copy to the FAT boot partition:

- `circle-zx/kernel8-32.img`
- `circle-zx/config.txt`
- Circle boot firmware files from `circle/boot/` (`bootcode.bin`, `start.elf`, `fixup.dat`, DTBs)

If needed:

```bash
cd circle/boot
make
```

## Game Files

Put `.z80`, `.tap`, or `.tzx` files in the SD card root directory used by `emmc1-1` (the OSD scan path).

Optional 128K ROM support:

- Circle frontend: place `128-0.rom` and optionally `128-1.rom` in the SD card root.
- SDL frontend: place `128-0.rom` and optionally `128-1.rom` in `roms/`.
- Each ROM file must be exactly 16KB. If only `128-0.rom` is present, it is used for both ROM banks.

## Controls

- **F1**: open/close OSD
- **F2**: reset
- **F3**: play/restart tape
- **F4**: stop tape
- **F6**: toggle turbo tape
- **Arrow keys + Tab**: Kempston joystick
- **Shift/Ctrl**: CAPS SHIFT / SYMBOL SHIFT

Inside OSD:

- **Up/Down**: select file
- **Enter**: toggle keyboard layout (first item) or load selected file
- **Esc/F1**: close OSD

## TAP/TZX Loading Flow

1. Open OSD (`F1`)
2. Select `.tap` or `.tzx` and press `Enter`
3. In Spectrum BASIC type `LOAD ""`
4. Press `F3` to start tape
5. Optionally enable turbo with `F6`

## Notes

- `.z80` support includes 48K snapshots plus native 128K RAM-bank restore and `7FFD` paging.
- The bundled ROM image is still the 48K ROM; by default 128K mode uses it for both ROM slots. The core now exposes `zx_set_128k_roms()` and the frontends can optionally load external `128-0.rom` / `128-1.rom` images.
- AY tone, noise, and basic envelope shaping are mixed into the mono audio output for 128K snapshots. AY timing and levels are usable now, but the PSG model is still approximate rather than cycle-accurate.

## License

GPLv3. See `LICENSE`.
