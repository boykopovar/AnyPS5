from pathlib import Path
import struct
import subprocess
import sys
import tempfile


DT_NEEDED = 1
DT_PLTRELSZ = 2
DT_PLTGOT = 3
DT_STRTAB = 5
DT_SYMTAB = 6
DT_RELA = 7
DT_RELASZ = 8
DT_RELAENT = 9
DT_STRSZ = 10
DT_SYMENT = 11
DT_PLTREL = 20
DT_JMPREL = 23
DT_OS_PLTGOT = 0x61000027
DT_OS_JMPREL = 0x61000029
DT_OS_PLTREL = 0x6100002B
DT_OS_PLTRELSZ = 0x6100002D
DT_OS_SYMTABSZ = 0x6100003F


def fixture(jmprel=0x700, pltrelsz=24, os_tags=True, target=0x300):
    image = bytearray(0x1000)
    image[:16] = b"\x7fELF\x02\x01\x01" + bytes(9)
    struct.pack_into("<HHIQQQIHHHHHH", image, 16,
                     3, 62, 1, 0x200, 64, 0, 0, 64, 56, 3, 64, 0, 0)
    image[0x200:0x206] = b"\xff\x25\xfa\x00\x00\x00"
    tags = [
        (DT_NEEDED, 1),
        (DT_STRTAB, 0x600),
        (DT_STRSZ, 16),
        (DT_SYMTAB, 0x620),
        (DT_SYMENT, 24),
        (DT_OS_SYMTABSZ, 48),
        (DT_RELA, 0x7A0),
        (DT_RELASZ, 0),
        (DT_RELAENT, 24),
        (DT_OS_PLTGOT if os_tags else DT_PLTGOT, 0x300),
        (DT_OS_PLTREL if os_tags else DT_PLTREL, DT_RELA),
        (DT_OS_JMPREL if os_tags else DT_JMPREL, jmprel),
        (DT_OS_PLTRELSZ if os_tags else DT_PLTRELSZ, pltrelsz),
        (0, 0),
    ]
    struct.pack_into("<IIQQQQQQ", image, 64,
                     1, 7, 0, 0, 0, len(image), len(image), 0x1000)
    struct.pack_into("<IIQQQQQQ", image, 120,
                     2, 6, 0x400, 0x400, 0x400, len(tags) * 16, len(tags) * 16, 8)
    struct.pack_into("<IIQQQQQQ", image, 176,
                     0x61000000, 0, 0, 0, 0, len(image), len(image), 1)
    for index, tag in enumerate(tags):
        struct.pack_into("<qQ", image, 0x400 + index * 16, *tag)
    struct.pack_into("<QQq", image, 0x700, target, (1 << 32) | 7, 0)
    struct.pack_into("<I", image, 0x620 + 24, 8)
    image[0x600:0x610] = b"\x00lib.so\x00symbol\x00\x00"[:16]
    return image


def run(relinker, work, name, image, expected_error=None):
    source = work / (name + ".elf")
    output = work / (name + ".out")
    source.write_bytes(image)
    result = subprocess.run([str(relinker), "--skip-sce-module", "--windows", str(source), str(output)],
                            capture_output=True, text=True, timeout=20)
    if expected_error is None:
        assert result.returncode == 0 and output.exists(), (name, result.stdout, result.stderr)
    else:
        assert result.returncode == 2 and expected_error in result.stderr and not output.exists(), (
            name, result.returncode, result.stdout, result.stderr)


def main():
    relinker = Path(sys.argv[1]).resolve()
    error = "Jump relocation table is out of bounds"
    with tempfile.TemporaryDirectory(prefix="anyps5-jmprel-table-") as directory:
        work = Path(directory)
        run(relinker, work, "os-valid", fixture())
        run(relinker, work, "sysv-valid", fixture(os_tags=False))
        run(relinker, work, "empty-table", fixture(pltrelsz=0))
        run(relinker, work, "offset-max-empty-table",
            fixture(jmprel=0xFFFFFFFFFFFFFFFF, pltrelsz=0), error)
        run(relinker, work, "offset-wrap-empty-table",
            fixture(jmprel=0xFFFFFFFFFFFFFFF0, pltrelsz=0), error)
        run(relinker, work, "offset-past-eof-empty-table",
            fixture(jmprel=0x1001, pltrelsz=0), error)
        run(relinker, work, "offset-max", fixture(jmprel=0xFFFFFFFFFFFFFFFF), error)
        run(relinker, work, "offset-wrap", fixture(jmprel=0xFFFFFFFFFFFFFFF0), error)
        run(relinker, work, "size-past-eof", fixture(jmprel=0xFF0), error)
        run(relinker, work, "size-max", fixture(pltrelsz=0xFFFFFFFFFFFFFFF0), error)
        for target in (0xFFFFFFFFFFFFFFF8, 0xFFFFFFFFFFFFFFFC, 0xFFFFFFFFFFFFFFFF):
            run(relinker, work, f"target-wrap-{target:x}", fixture(target=target),
                "Relocation target range exceeds the address space")
    print("Jump relocation table bounds tests passed")


if __name__ == "__main__":
    main()
