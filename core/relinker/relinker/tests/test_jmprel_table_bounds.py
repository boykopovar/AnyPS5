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


TARGET_OPTIONS = {
    "windows": ["--windows"],
    "macos": ["--macos"],
    "lazy": ["--lazy-binding"],
    "fixed": ["--linux-fixed-base", "0x10000000"],
}


def fixture(jmprel=0x700, pltrelsz=24, os_tags=True, target=0x300, symtab=0x620,
            symbol_bytes=48, symbol_index=1, relocation_type=None, data_relocations=False, linux=False,
            addend=0):
    image = bytearray(0x1000)
    if relocation_type is None:
        relocation_type = 6 if data_relocations else 7
    image[:16] = b"\x7fELF\x02\x01\x01" + bytes(9)
    struct.pack_into("<HHIQQQIHHHHHH", image, 16,
                     3, 62, 1, 0x200, 64, 0, 0, 64, 56, 8 if linux else 3, 64, 0, 0)
    image[0x200:0x206] = b"\xff\x25\xfa\x00\x00\x00"
    tags = [
        (DT_NEEDED, 1),
        (DT_STRTAB, 0x600),
        (DT_STRSZ, 16),
        (DT_SYMTAB, symtab),
        (DT_SYMENT, 24),
        (DT_OS_SYMTABSZ, symbol_bytes),
        (DT_RELA, 0x700 if data_relocations else 0x7A0),
        (DT_RELASZ, 24 if data_relocations else 0),
        (DT_RELAENT, 24),
        (DT_OS_PLTGOT if os_tags else DT_PLTGOT, 0x300),
        (DT_OS_PLTREL if os_tags else DT_PLTREL, DT_RELA),
        (DT_OS_JMPREL if os_tags else DT_JMPREL, jmprel),
        (DT_OS_PLTRELSZ if os_tags else DT_PLTRELSZ, 0 if data_relocations else pltrelsz),
        (0, 0),
    ]
    if symbol_bytes is None:
        tags = [tag for tag in tags if tag[0] != DT_OS_SYMTABSZ]
    struct.pack_into("<IIQQQQQQ", image, 64,
                     1, 7, 0, 0, 0, len(image), len(image), 0x1000)
    struct.pack_into("<IIQQQQQQ", image, 120,
                     2, 6, 0x400, 0x400, 0x400, len(tags) * 16, len(tags) * 16, 8)
    struct.pack_into("<IIQQQQQQ", image, 176,
                     0x61000000, 0, 0, 0, 0, len(image), len(image), 1)
    if linux:
        for index in range(3, 8):
            struct.pack_into("<I", image, 64 + index * 56, 0x6fffff01)
    for index, tag in enumerate(tags):
        struct.pack_into("<qQ", image, 0x400 + index * 16, *tag)
    struct.pack_into("<QQq", image, 0x700, target, (symbol_index << 32) | relocation_type, addend)
    symbol = symtab + symbol_index * 24
    if symbol <= len(image) - 4:
        struct.pack_into("<I", image, symbol, 8)
    if relocation_type == 8 and symbol <= len(image) - 24:
        image[symbol + 4] = 3
    image[0x600:0x610] = b"\x00lib.so\x00symbol\x00\x00"[:16]
    return image


def run(relinker, work, name, image, expected_error=None, target="windows", filter_level=None):
    source = work / (name + ".elf")
    output = work / (name + ".out")
    source.write_bytes(image)
    options = list(TARGET_OPTIONS.get(target, []))
    if filter_level is not None:
        options.append(f"unused-filter={filter_level}")
    result = subprocess.run([str(relinker), "--skip-sce-module", *options, str(source), str(output)],
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
        for output_target in ("linux", "windows"):
            for data in (False, True):
                options = {"linux": output_target == "linux", "data_relocations": data}
                prefix = f"{output_target}-{'data' if data else 'plt'}-symbol"
                run(relinker, work, prefix + "-valid", fixture(**options), target=output_target)
                run(relinker, work, prefix + "-past-declared-table", fixture(symbol_bytes=24, **options),
                    "Symbol table entry out of bounds", target=output_target)
                run(relinker, work, prefix + "-truncated", fixture(symtab=0xFE3, symbol_bytes=None, **options),
                    "Symbol table entry out of bounds", target=output_target)
                if data:
                    run(relinker, work, prefix + "-relative-valid", fixture(relocation_type=8, **options),
                        target=output_target)
                    run(relinker, work, prefix + "-relative-past-declared-table",
                        fixture(symbol_bytes=24, relocation_type=8, **options),
                        "Symbol table entry out of bounds", target=output_target)
                    run(relinker, work, prefix + "-none", fixture(symbol_bytes=0, symbol_index=0xFFFFFFFF,
                                                                 relocation_type=0, **options), target=output_target)
                else:
                    run(relinker, work, prefix + "-unsupported-plt-type", fixture(relocation_type=6, **options),
                        "Unsupported PLT relocation type", target=output_target)
                    run(relinker, work, prefix + "-relative-in-plt", fixture(relocation_type=8, **options),
                        "Unsupported PLT relocation type", target=output_target)
                    run(relinker, work, prefix + "-none-in-plt", fixture(relocation_type=0, symbol_index=0, **options),
                        "Unsupported PLT relocation type", target=output_target)
                    for level in (1, 2):
                        run(relinker, work, f"{prefix}-none-in-plt-filter-{level}",
                            fixture(relocation_type=0, symbol_index=0, **options),
                            "Unsupported PLT relocation type", target=output_target, filter_level=level)
        for output_target in ("linux", "fixed", "lazy", "windows", "macos"):
            for data in (False, True):
                prefix = f"{output_target}-{'glob-dat' if data else 'jump-slot'}-addend"
                run(relinker, work, prefix + "-zero", fixture(linux=True, data_relocations=data), target=output_target)
                for addend in (0x10, -8):
                    run(relinker, work, f"{prefix}-{addend:x}", fixture(linux=True, data_relocations=data, addend=addend),
                        "GLOB_DAT/JUMP_SLOT relocation has a nonzero addend (offset 0x300)", target=output_target)
    print("Jump relocation table bounds tests passed")


if __name__ == "__main__":
    main()
