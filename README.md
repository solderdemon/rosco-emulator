# rosco-emulator

An emulator for the [rosco_m68k](https://github.com/rosco-m68k/rosco_m68k) family of homebrew
single board computers, built as a slimmed-down MAME target.

This tree is MAME 0.289 with everything unrelated to rosco removed. The only
driver that remains lives in `src/rosco/drivers`, and the build produces a
single `rosco` executable instead of the full MAME binary.

The rosco driver originates from the SBC MAME fork at
[roscopeco/mame](https://github.com/roscopeco/mame), which is where the
rosco_m68k machines were first emulated. It has been ported forward to current
MAME here - see [Credits](#credits).

## Emulated systems

| Short name       | Description                    |
| ---------------- | ------------------------------ |
| `rosco_m68k_000` | rosco_m68k Classic V2, MC68000 |
| `rosco_m68k_010` | rosco_m68k Classic V2, MC68010 |
| `rosco_m68k_020` | rosco_m68k Classic V2, MC68020 |
| `rosco_m68k_030` | rosco_m68k Classic V2, MC68030 |

The Classic V2 machines emulate 1MB of DRAM, the XR68C681 DUART with both
serial channels, the bit-banged SPI SD card hanging off the DUART output port,
and the IDE/ATA interface. Unmapped addresses raise a bus error, which is what
the firmware uses to size memory and probe for expansion hardware.

CPU cores for both the 68000 family and the W65C02S are enabled in the build,
the latter so that a `rosco_6502` driver can be added later without touching the
target configuration.

## Building

Requires a C++20 compiler, Python 3, SDL2 and the usual MAME build dependencies.

```sh
make -j$(nproc)
```

`rosco` is the default target, so no `TARGET=` is needed. The resulting binary is
`./rosco` in the top of the tree.

## Firmware

The rosco_m68k firmware is built from source by its users rather than being a
fixed ROM dump, so `roms/rosco_m68k_<cpu>.zip` just needs to contain whatever
`rosco_m68k.rom` you want to run. The hashes in `src/rosco/drivers/rosco_m68k.cpp`
describe the build that this tree was tested against; if you drop in your own
firmware, either update them or start the emulator with `-novalidate`.

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
```

Raw images work as-is; `.chd` images can be created with `chdman`, built by
`make TOOLS=1`.

## Credits

The rosco_m68k drivers come from the SBC MAME fork,
[roscopeco/mame](https://github.com/roscopeco/mame), by Ross Bamford and
contributors, with portions by Chris Hanson. Porting them to MAME 0.289
required updating the bus error handling for the rewritten 68000 core and the
usual API churn; the hardware emulation itself is theirs.

The rosco_m68k computer itself is by
[The Really Old-School Company](https://github.com/rosco-m68k/rosco_m68k).

## License

MAME is distributed under the terms in `COPYING` (GPL-2.0-or-later). The rosco
drivers carry their own MIT / BSD-3-Clause notices in their source headers.
