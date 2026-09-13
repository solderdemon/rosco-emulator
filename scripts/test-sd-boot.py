#!/usr/bin/env python3
"""Exercise firmware SD loading across sectors and RAM banks, plus quickload."""

from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parent.parent


def program(message, banked=False):
    # Poll DUART A TxRDY and print a zero-terminated string, then RTS.
    # The message is deliberately outside the first sector/first RAM bank.
    code = bytes.fromhex("a2 00 bd 00 10 f0 0f 48 ad 01 c0 29 04 f0 f9 "
                         "68 8d 03 c0 e8 80 ec 60")
    offset = 0x800
    if banked:
        code = bytes.fromhex("a9 01 85 00") + code.replace(
            bytes.fromhex("bd 00 10"), bytes.fromhex("bd 00 40"))[:-1]
        code += bytes.fromhex("64 00 60")  # restore bank 0 before RTS
        offset = 0xB800  # low RAM plus all of bank 0; start of bank 1
    return code.ljust(offset, b"\0") + message.encode() + b"\r\n\0"


def main():
    with tempfile.TemporaryDirectory(prefix="rosco-boot-test-") as work:
        for mode, banked in (("-p", False), ("-p", True), ("-q", False)):
            marker = "SD BANK BOOT OK" if banked else "PROGRAM BOOT OK"
            binary = Path(work) / "program.bin"
            binary.write_bytes(program(marker, banked))
            result = subprocess.run(
                ["bash", str(ROOT / "scripts/rosco-test.sh"), "-m", "rosco_6502",
                 mode, str(binary), "-t", "20", "-e", marker],
                cwd=ROOT, text=True, capture_output=True)
            if result.returncode or marker not in result.stdout:
                raise RuntimeError(result.stdout + result.stderr)
            if mode == "-p" and ("Starting..." not in result.stdout or
                                  "rosco_6502 EWozMon" not in result.stdout):
                raise RuntimeError("SD program did not boot and return to monitor\n" + result.stdout)
            print(f"PASS {mode} {'RAM bank crossing' if banked else 'multiple sectors'}")


if __name__ == "__main__":
    main()
