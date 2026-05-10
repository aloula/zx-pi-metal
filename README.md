# ZX Spectrum 48K Bare Metal for Raspberry Pi 3B+

This project runs a ZX Spectrum 48K emulator directly on Raspberry Pi hardware using the [Circle](https://github.com/rsta2/circle) bare-metal environment.

The active frontend is `circle-zx/`.

## Features

- ZX Spectrum 48K emulation core
- Scaled framebuffer output
- USB keyboard input mapping
- Kempston joystick mapping (keyboard)
- OSD browser for `.z80`, `.tap`, `.tzx`
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
- **Enter**: load selected file
- **Esc/F1**: close OSD

## TAP/TZX Loading Flow

1. Open OSD (`F1`)
2. Select `.tap` or `.tzx` and press `Enter`
3. In Spectrum BASIC type `LOAD ""`
4. Press `F3` to start tape
5. Optionally enable turbo with `F6`

## Notes

- `.z80` support is 48K snapshots only.
- CP/M is out of scope for this bare-metal frontend.

## License

GPLv3. See `LICENSE`.
