from pathlib import Path
import struct
import subprocess
import sys
import tempfile

from test_guest_intel_trampolines import elf_loads, main_fixture, pe_sections
from test_guest_module_directories import module_with_symbol


ABSOLUTE = 0xFFF1
COMMON = 0xFFF2
VALUE = 0x1234
ADDEND = 0x10
TARGET = 0x2320


def module_with_section(section):
    image = module_with_symbol(True)
    struct.pack_into("<HQQ", image, 0x898 + 6, section, VALUE, 0)
    struct.pack_into("<Q", image, 0x668, 24)
    struct.pack_into("<QQq", image, 0x900, TARGET, (1 << 32) | 1, ADDEND)
    return image


def pe_qwords(data):
    for name, rva, raw_size, raw_offset, _ in pe_sections(data):
        for position in range(0, raw_size - 7, 8):
            yield rva + position, struct.unpack_from("<Q", data, raw_offset + position)[0]


def elf_relocation(data):
    loads = elf_loads(data)
    phoff, = struct.unpack_from("<Q", data, 32)
    phsize, phcount = struct.unpack_from("<HH", data, 54)
    dynamic = next(struct.unpack_from("<IIQQQQQQ", data, phoff + index * phsize)
                   for index in range(phcount)
                   if struct.unpack_from("<I", data, phoff + index * phsize)[0] == 2)

    def offset(address):
        for header in loads:
            if header[3] <= address < header[3] + header[5]:
                return header[2] + address - header[3]
        raise AssertionError(f"Unmapped address: {address:#x}")

    tags = {}
    for position in range(dynamic[2], dynamic[2] + dynamic[5], 16):
        tag, value = struct.unpack_from("<qQ", data, position)
        if tag == 0:
            break
        tags.setdefault(tag, value)
    symbols = offset(tags[6])
    relocations = offset(tags[7])
    for position in range(relocations, relocations + tags[8], 24):
        target, info, addend = struct.unpack_from("<QQq", data, position)
        if target == TARGET:
            section, value = struct.unpack_from("<HQ", data, symbols + (info >> 32) * 24 + 6)
            return info & 0xFFFFFFFF, section, value, addend
    raise AssertionError("Relocation against the absolute symbol is missing")


def main():
    relinker = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="anyps5-guest-absolute-") as directory:
        work = Path(directory)

        def convert(name, windows, section):
            case = work / f"{name}-{windows}"
            (case / "sce_module").mkdir(parents=True)
            (case / "sce_module" / "module.prx").write_bytes(module_with_section(section))
            source = case / "input.elf"
            source.write_bytes(main_fixture())
            output = case / ("output.exe" if windows else "output.elf")
            result = subprocess.run([str(relinker), *(["--windows"] if windows else []), str(source), str(output)],
                                    capture_output=True, text=True, timeout=30)
            return result, case / "app0" / "sce_module" / "module.prx.guest.prx"

        for windows in (False, True):
            result, artifact = convert("absolute", windows, ABSOLUTE)
            assert result.returncode == 0, (result.stdout, result.stderr)
            data = artifact.read_bytes()
            if windows:
                assert any(value == VALUE + ADDEND for _, value in pe_qwords(data)), "absolute relocation value missing"
            else:
                assert elf_relocation(data) == (1, ABSOLUTE, VALUE, ADDEND)

            result, artifact = convert("common", windows, COMMON)
            assert result.returncode == 2 and "Unsupported special symbol section" in result.stderr, result.stderr
            assert not artifact.exists(), artifact
    print("Guest absolute symbol tests passed")


if __name__ == "__main__":
    main()
