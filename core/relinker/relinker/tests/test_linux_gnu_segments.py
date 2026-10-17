"""Check Linux output preserves GNU stack and RELRO program headers."""

from pathlib import Path
import struct
import subprocess
import sys
import tempfile

from test_linux_load_alignment import fixture

PT_GNU_STACK = 0x6474E551
PT_GNU_RELRO = 0x6474E552
PT_SCE_VERSION = 0x6FFFFF01
BASE = 0x400000


def image():
    elf = fixture()
    struct.pack_into("<H", elf, 56, 10)
    for index in range(2, 10):
        struct.pack_into("<IIQQQQQQ", elf, 64 + index * 56, PT_SCE_VERSION, 0, 0, 0, 0, 0, 0, 1)
    struct.pack_into("<IIQQQQQQ", elf, 64 + 2 * 56, PT_GNU_STACK, 6, 0, 0, 0, 0, 0, 16)
    struct.pack_into("<IIQQQQQQ", elf, 64 + 3 * 56, PT_GNU_RELRO, 4, 0x4500, 0x500, 0x500, 0x100, 0x100, 1)
    return elf


def headers(elf):
    offset, = struct.unpack_from("<Q", elf, 0x20)
    entry_size, count = struct.unpack_from("<HH", elf, 0x36)
    return [struct.unpack_from("<IIQQQQQQ", elf, offset + index * entry_size) for index in range(count)]


def main():
    relinker = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="anyps5-linux-gnu-segments-") as directory:
        work = Path(directory)
        source = work / "input.elf"
        source.write_bytes(image())
        for options, base, keeps_relro in (([], 0, True), (["--linux-fixed-base", hex(BASE)], BASE, True), (["--lazy-binding"], 0, False)):
            output = work / ("fixed.elf" if base else "plain.elf")
            result = subprocess.run([str(relinker), "--skip-sce-module", *options, str(source), str(output)],
                                    capture_output=True, text=True, timeout=20)
            assert result.returncode == 0, (result.stdout, result.stderr)
            found = {header[0]: header for header in headers(output.read_bytes())}
            assert PT_GNU_STACK in found, "PT_GNU_STACK was dropped"
            assert (PT_GNU_RELRO in found) == keeps_relro, "PT_GNU_RELRO presence does not match the binding mode"
            stack = found[PT_GNU_STACK]
            assert stack[1] == 6 and stack[2:6] == (0, 0, 0, 0), stack
            if keeps_relro:
                relro = found[PT_GNU_RELRO]
                assert relro[3:5] == (0x500 + base, 0x500 + base), relro
                assert relro[5:7] == (0x100, 0x100), relro
    print("Linux GNU segment tests passed")


if __name__ == "__main__":
    main()
