from pathlib import Path
import struct
import subprocess
import sys
import tempfile

from test_guest_intel_trampolines import elf_loads, main_fixture, pe_sections
from test_guest_module_directories import module_with_symbol
from test_macos_output import MachO


ABSOLUTE = 0xFFF1
COMMON = 0xFFF2
VALUE = 0x1234
SIZE = 0x10
ADDEND = 0x10
TARGET = 0x2320
TLS_SECTIONS = (ABSOLUTE, COMMON, 0xFF00, 0xFFFF)


def module_with_section(section, size=0):
    image = module_with_symbol(True)
    struct.pack_into("<HQQ", image, 0x898 + 6, section, VALUE, size)
    struct.pack_into("<Q", image, 0x668, 24)
    struct.pack_into("<QQq", image, 0x900, TARGET, (1 << 32) | 1, ADDEND)
    return image


def tls_module(section, binding):
    image = module_with_symbol(True)
    struct.pack_into("<H", image, 56, 4)
    struct.pack_into("<IIQQQQQQ", image, 232, 7, 4, 0x980, 0x2380, 0x2380, 8, 0x40, 16)
    struct.pack_into("<BBHQQ", image, 0x898 + 4, binding << 4 | 6, 0, section, 0, 8)
    struct.pack_into("<Q", image, 0x668, 24)
    struct.pack_into("<QQq", image, 0x900, TARGET, (1 << 32) | 17, 0)
    return image


def pe_qwords(data):
    for name, rva, raw_size, raw_offset, _ in pe_sections(data):
        for position in range(0, raw_size - 7, 8):
            yield rva + position, struct.unpack_from("<Q", data, raw_offset + position)[0]


def macho_qwords(data):
    for segment in MachO(data).segments:
        if not segment["name"].startswith("__ELF"):
            continue
        for position in range(0, segment["filesize"] - 7, 8):
            yield segment["vmaddr"] + position, struct.unpack_from("<Q", data, segment["fileoff"] + position)[0]


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

        def convert(name, system, image):
            case = work / f"{name}-{system}"
            (case / "sce_module").mkdir(parents=True)
            (case / "sce_module" / "module.prx").write_bytes(image)
            source = case / "input.elf"
            source.write_bytes(main_fixture())
            output = case / ("output.exe" if system == "windows" else "output.elf")
            flags = {"linux": [], "windows": ["--windows"], "macos": ["--macos"]}[system]
            result = subprocess.run([str(relinker), *flags, str(source), str(output)],
                                    capture_output=True, text=True, timeout=30)
            return result, case / "app0" / "sce_module" / "module.prx.guest.prx"

        for system in ("linux", "windows", "macos"):
            for size in (0, SIZE):
                result, artifact = convert(f"absolute-{size}", system, module_with_section(ABSOLUTE, size))
                assert result.returncode == 0, (system, size, result.stdout, result.stderr)
                data = artifact.read_bytes()
                if system == "windows":
                    assert any(value == VALUE + ADDEND for _, value in pe_qwords(data)), "absolute relocation value missing"
                elif system == "macos":
                    assert any(value == VALUE + ADDEND for _, value in macho_qwords(data)), "absolute relocation value missing"
                else:
                    assert elf_relocation(data) == (1, ABSOLUTE, VALUE, ADDEND)

            result, artifact = convert("common", system, module_with_section(COMMON))
            assert result.returncode == 2 and "Unsupported special symbol section" in result.stderr, result.stderr
            assert not artifact.exists(), artifact

            for binding in (0, 1):
                result, artifact = convert(f"tls-1-{binding}", system, tls_module(1, binding))
                assert result.returncode == 0 and artifact.exists(), (system, binding, result.stderr)
                for section in TLS_SECTIONS:
                    result, artifact = convert(f"tls-{section:x}-{binding}", system, tls_module(section, binding))
                    assert result.returncode == 2 and "Unsupported special symbol section" in result.stderr, (system, hex(section), binding, result.stderr)
                    assert not artifact.exists(), artifact
    print("Guest absolute symbol tests passed")


if __name__ == "__main__":
    main()
