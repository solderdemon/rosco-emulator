#!/usr/bin/env python3
"""Build and package the rosco_6502 ROM from a local hardware repository."""

import argparse
import hashlib
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import zipfile
import zlib


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("repository", type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parent.parent
    source = args.repository.resolve() / "code/firmware"
    with tempfile.TemporaryDirectory(prefix="rosco-rom-") as work:
        build = Path(work) / "firmware"
        shutil.copytree(source, build)
        # Normalize CRLF from Windows checkouts in the temporary copy.
        fat = build / "fat32_readonly.s"
        fat.write_text(fat.read_text(), newline="\n")
        subprocess.run(["patch", "--batch", "-p1"], cwd=build, check=True,
                       input=(root / "scripts/firmware-fat32.patch").read_text(), text=True)
        for bank in range(4):
            subprocess.run(["ca65", "-g", "-D__ROSCO__=1", "--cpu", "65SC02",
                            "--feature", "string_escapes", "-I", "inc",
                            f"bank{bank}.s", "-o", f"bank{bank}.o"], cwd=build, check=True)
        subprocess.run(["ld65", "-C", "rosco_6502_32K.cfg",
                        "bank0.o", "bank1.o", "bank2.o", "bank3.o", "-o", "boot32k"],
                       cwd=build, check=True)
        banks = [(build / f"boot32k.{i}.bin").read_bytes() for i in range(4)]
        if any(len(bank) != 8192 for bank in banks):
            raise ValueError("expected four 8 KiB ROM banks")
        rom = b"".join(banks)

    crc = f"{zlib.crc32(rom):08x}"
    sha = hashlib.sha1(rom).hexdigest()
    driver = root / "src/rosco/drivers/rosco_6502.cpp"
    text, count = re.subn(r"CRC\([0-9a-f]+\) SHA1\([0-9a-f]+\)",
                         f"CRC({crc}) SHA1({sha})", driver.read_text())
    if count != 1:
        raise ValueError("expected one ROM checksum in driver")
    with zipfile.ZipFile(root / "roms/rosco_6502.zip", "w") as archive:
        info = zipfile.ZipInfo("rosco_6502.rom", date_time=(2026, 9, 13, 0, 0, 0))
        info.compress_type = zipfile.ZIP_DEFLATED
        archive.writestr(info, rom)
    driver.write_text(text, newline="\n")
    print(f"Updated rosco_6502 ROM: CRC {crc}, SHA1 {sha}")


if __name__ == "__main__":
    main()
