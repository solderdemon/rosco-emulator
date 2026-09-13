#!/usr/bin/env python3
"""Create a partitioned FAT32 boot card for the rosco_6502 firmware."""

import argparse
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("program", type=Path, help="binary linked at $0800")
    parser.add_argument("output", type=Path, help="new SD image (must not exist)")
    args = parser.parse_args()
    if not args.program.is_file():
        parser.error(f"no such program: {args.program}")
    if not 0 < args.program.stat().st_size <= 0x80000:
        parser.error("program must contain 1 to 524288 bytes (512 KiB)")
    if args.output.exists():
        parser.error(f"output already exists: {args.output}")
    for tool in ("mkfs.fat", "mcopy"):
        if shutil.which(tool) is None:
            parser.error(f"{tool} is required; install dosfstools and mtools")

    # A 64 MiB FAT32 volume, preceded by a 1 MiB alignment gap and an MBR.
    # Firmware expects a partition table, rather than a bare FAT volume.
    start, sectors = 2048, 131072
    with tempfile.TemporaryDirectory(prefix="rosco-fat-") as work:
        volume = Path(work) / "volume.img"
        with volume.open("wb") as stream:
            stream.truncate(sectors * 512)
        subprocess.run(["mkfs.fat", "-F", "32", "-S", "512", "-s", "1",
                        "-h", str(start), "-n", "ROSCO", str(volume)],
                       check=True, stdout=subprocess.DEVNULL)
        # mcopy emits the VFAT long filename required by the firmware.
        subprocess.run(["mcopy", "-i", str(volume), str(args.program.resolve()),
                        "::/ROSC0DE_6502.BIN"], check=True)
        mbr = bytearray(512)
        struct.pack_into("<B3sB3sII", mbr, 446, 0, b"\xfe\xff\xff", 0x0C,
                         b"\xfe\xff\xff", start, sectors)
        mbr[510:512] = b"\x55\xaa"
        with args.output.open("xb") as stream:
            stream.write(mbr)
            stream.seek(start * 512)
            with volume.open("rb") as source:
                shutil.copyfileobj(source, stream)


if __name__ == "__main__":
    main()
