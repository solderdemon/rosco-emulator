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

The hashes in the drivers describe the builds this tree was tested against. A
different firmware only gets a `WRONG CHECKSUMS` warning and still runs, so
there is no need to keep them in step while developing.

## Testing firmware and programs

`scripts/rosco-test.sh` boots a machine headless and prints whatever came out
of the console serial port. It can run a firmware image, a program, or both.

**A firmware image** is staged into a temporary ROM path, so `roms/` is left
alone and the checksums in the drivers do not matter:

```sh
scripts/rosco-test.sh boot32k.bin
scripts/rosco-test.sh -m rosco_m68k_010 -s sdcard.img rosco_m68k.rom
```

**A program** goes straight into memory with `-q`, no Kermit and no SD card
needed. The firmware is left to finish booting first, then the program is
loaded and jumped to:

```sh
scripts/rosco-test.sh -q hello.bin                      # on the stock firmware
scripts/rosco-test.sh -q hello.bin boot32k.bin          # on your own firmware
```

Programs load at the address they are linked for: `$0800` on the rosco_6502 and
`0x40000` on the rosco_m68k. Note that a bare program says nothing about which
machine it is for, so pass `-m` unless the default guess is right.

**`-e` turns a run into a pass/fail check**, which is what makes this usable
from a Makefile or CI - it exits non-zero if the string never appears:

```sh
scripts/rosco-test.sh -e 'Memory checks: passed' boot32k.bin
scripts/rosco-test.sh -q hello.bin -e 'hello, world'
```

The firmware's own power-on self test walks every RAM and ROM bank, so the
first of those is a real smoke test of the whole memory map.

Other options: `-t SECONDS` for how long to run (default 8 emulated seconds,
about a second of wall clock), `-i` for a window instead of headless, `-r` to
keep the ANSI escapes, and `--` to pass anything else through to the emulator.

The emulator takes `-quik` directly too, if you would rather not go through the
script:

```sh
./rosco rosco_6502 -quik hello.bin
```

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
