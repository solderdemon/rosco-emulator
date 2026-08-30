# rosco-emulator

An emulator for the [rosco_m68k](https://github.com/rosco-m68k/rosco_m68k) and
[rosco_6502](https://github.com/rosco-6502/rosco_6502) homebrew single board
computers, built as a slimmed-down MAME target.

This tree is MAME 0.289 with everything unrelated to rosco removed. The only
drivers that remain live in `src/rosco/drivers`, and the build produces a
single `rosco` executable instead of the full MAME binary.

The rosco_m68k driver originates from the SBC MAME fork at
[roscopeco/mame](https://github.com/roscopeco/mame), which is where these
machines were first emulated, and has been ported forward to current MAME here.
The rosco_6502 driver is new - see [Credits](#credits).

## Emulated systems

| Short name       | Description                    |
| ---------------- | ------------------------------ |
| `rosco_m68k_000` | rosco_m68k Classic V2, MC68000 |
| `rosco_m68k_010` | rosco_m68k Classic V2, MC68010 |
| `rosco_m68k_020` | rosco_m68k Classic V2, MC68020 |
| `rosco_m68k_030` | rosco_m68k Classic V2, MC68030 |
| `rosco_6502`     | rosco_6502 r4, W65C02S         |

The **Classic V2** machines emulate 1MB of DRAM, the XR68C681 DUART with both
serial channels, the bit-banged SPI SD card hanging off the DUART output port,
and the IDE/ATA interface. Unmapped addresses raise a bus error, which is what
the firmware uses to size memory and probe for expansion hardware.

The **rosco_6502** emulates the W65C02S at 14MHz, 16KB of low RAM, 16 banks of
32KB high RAM (512KB), 4 banks of 8KB ROM and the same XR68C681 DUART and SPI
SD card wiring as its bigger sibling. Banking goes through the register at
$0000, which the address decoder mirrors at $0001 and reads back from the RAM
underneath. The firmware's own power-on self test walks every RAM and ROM bank,
so a clean boot exercises the whole memory map.

### Known limitations

The firmware measures CPU speed against the DUART timer and prints the result
at boot. It comes out right for the 68000, 68010 and W65C02S, but the 68020 and
68030 read about twice their configured 20MHz: MAME's Musashi cores do not
model the prefetch and cache behaviour those parts have, so the calibration
loop runs through more iterations than it would on real silicon.

## Building

Requires a C++20 compiler, Python 3, SDL2 and the usual MAME build dependencies.

```sh
make -j$(nproc)
```

`rosco` is the default target, so no `TARGET=` is needed. The resulting binary is
`./rosco` in the top of the tree.

## Firmware

The rosco firmware is built from source by its users rather than being a fixed
ROM dump, so the ROM sets just need to contain whatever firmware you want to
run:

| ROM set                    | Contents                                       |
| -------------------------- | ---------------------------------------------- |
| `roms/rosco_m68k_<cpu>.zip`| `rosco_m68k.rom`                               |
| `roms/rosco_6502.zip`      | `rosco_6502.rom`, the four 8KB banks concatenated |

The hashes in the drivers describe the builds this tree was tested against; if
you drop in your own firmware, either update them or start the emulator with
`-novalidate`.

## Running

```sh
# interactive, with the built-in terminal
./rosco rosco_m68k_010

# headless, with the console redirected to a file
./rosco rosco_m68k_010 -terminal null_modem -bitb console.txt -video none

# attach a pseudo-terminal instead, and talk to it with screen/minicom
./rosco rosco_m68k_010 -terminal pty
```

Both serial ports default to 115200 8N1, matching the firmware.

An SD card image goes on the first hard disk slot and the IDE disk on the second:

```sh
./rosco rosco_m68k_010 -hard1 sdcard.img -hard2 ide.chd
./rosco rosco_6502 -hard1 sdcard.img
```

Raw images work as-is; `.chd` images can be created with `chdman`, built by
`make TOOLS=1`. The rosco_6502 has no IDE interface, so it only takes the SD
card.

## Credits

The rosco_m68k drivers come from the SBC MAME fork,
[roscopeco/mame](https://github.com/roscopeco/mame), by Ross Bamford and
contributors, with portions by Chris Hanson. Porting them to MAME 0.289
required updating the bus error handling for the rewritten 68000 core and the
usual API churn; the hardware emulation itself is theirs.

The rosco_6502 driver was written for this tree from the board's schematic,
PLD equations and firmware sources, which document the memory map and the DUART
pin assignments in full.

The rosco computers themselves are by The Really Old-School Company:
[rosco_m68k](https://github.com/rosco-m68k/rosco_m68k) and
[rosco_6502](https://github.com/rosco-6502/rosco_6502).

## License

MAME is distributed under the terms in `COPYING` (GPL-2.0-or-later). The rosco
drivers carry their own MIT / BSD-3-Clause notices in their source headers.
