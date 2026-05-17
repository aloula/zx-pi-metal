# ZX Spectrum Bare Metal for Raspberry Pi 3B+

This project combines a high-performance Z80 emulator core with a Raspberry Pi bare-metal frontend built on the [Circle](https://github.com/rsta2/circle) environment.

## Features

- **Dual-Model Support**: Native ZX Spectrum 48K and 128K (Toastrack/Grey +2) emulation.
- **Cycle-Accurate Audio**: Mid-frame AY-3-8910 event logging for perfect software envelopes and PCM effects.
- **Hardware Quirks**: Full Floating Bus emulation (fixes *Arkanoid* and other beam-synced games).
- **Bare-Metal Performance**: Boots in seconds with low-latency input and audio.
- **OSD File Browser**: Integrated browser for `.z80` snapshots, `.tap`, and `.tzx` tapes.
- **Modern Input**: Support for USB keyboards and XBox 360-style gamepads (mapped to Kempston).
- **Visuals**: Scaled RGB framebuffer with border effects.

## Project Structure

- `src/`: Core emulator logic (Z80, ULA, AY-PSG, TZX).
- `frontends/bare-metal/`: Raspberry Pi specific code (Circle environment).
- `docs/`: Technical specifications and hardware reference.
- `tests/`: Automated test suite for CPU and ULA correctness.

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
make circle_zx
```

Generated image: `frontends/bare-metal/kernel8-32.img`

## SD Card Setup

Copy the following to the FAT root of your SD card:

1.  **Kernel**: `frontends/bare-metal/kernel8-32.img` (rename to `kernel8-32.img`).
2.  **Config**: `frontends/bare-metal/config.txt`.
3.  **Firmware**: Circle boot files from `circle/boot/` (`bootcode.bin`, `start.elf`, `fixup.dat`, etc).
4.  **ROMs**:
    *   `128-0.rom` and `128-1.rom` (16KB each) for 128K mode.
    *   Place these in the SD card root.
    *   If no 128K ROMs are found, it defaults to 48K mode using the internal ROM.
5.  **Games**: Place `.z80`, `.tap`, or `.tzx` files in the root or subdirectories.

## Controls

- **F1**: Open/Close OSD
- **F2**: Hard Reset
- **F3**: Play/Restart Tape
- **F4**: Stop Tape
- **F6**: Toggle Turbo Tape (4x speed)
- **Arrow keys + Tab**: Kempston Joystick (Keyboard)
- **Gamepad**: Left Stick/D-Pad + A/B/X/Y buttons
- **Shift/Ctrl**: CAPS SHIFT / SYMBOL SHIFT

## TAP/TZX Loading Flow

1. Open OSD (`F1`)
2. Select tape file and press `Enter`.
3. In Spectrum BASIC, type `LOAD ""` (shortcut: `J` then `Ctrl+P`, `Ctrl+P`).
4. Press `F3` to start the tape playback.
5. Use `F6` to fast-forward through loading screens.

## License

GPLv3. See `docs/LICENSE`.
