from pathlib import Path
import os
import struct
import subprocess
import sys
import tempfile

from test_guest_intel_trampolines import main_fixture, pe_sections
from test_guest_none_relocations import FUNCTION, TARGET, change_tag, module
from test_guest_symbol_names import dynamic_tags, file_offset
from test_macos_output import MachO


DTPMOD64 = 16
DTPOFF64 = 17
TPOFF64 = 18
TEMPLATE = 0x2780


def fixture(sce=False, tls=True, info=3, value=0, kind=DTPMOD64, addend=0, section=0, symbol=3, tls_memory=16, name=0):
    image = module([(TARGET, (symbol << 32) | kind, addend)], sce=sce)
    struct.pack_into("<I", image, 0x2244, 4)
    if sce:
        change_tag(image, 4, 4 * 24, 0x6100003f)
    struct.pack_into("<IBBHQQ", image, 0x2280 + 3 * 24, name, info, 0, section, value, 0)
    if tls:
        count, = struct.unpack_from("<H", image, 56)
        struct.pack_into("<H", image, 56, count + 1)
        struct.pack_into("<IIQQQQQQ", image, 64 + count * 56, 7, 4, TEMPLATE, TEMPLATE, TEMPLATE, min(8, tls_memory), tls_memory, 8)
        image[TEMPLATE:TEMPLATE + 8] = bytes(range(1, 9))
    return image


def with_flags(image, flags):
    size, = struct.unpack_from("<Q", image, 176 + 32)
    struct.pack_into("<qQqQ", image, 0x2000 + size - 16, 30, flags, 0, 0)
    struct.pack_into("<QQ", image, 176 + 32, size + 16, size + 16)
    return image


def relocations(data):
    tags = dynamic_tags(data)
    start = file_offset(data, tags[7])
    return [struct.unpack_from("<QQq", data, position) for position in range(start, start + tags[8], 24)]


def data_directory(data, index):
    header, = struct.unpack_from("<I", data, 0x3C)
    return struct.unpack_from("<II", data, header + 24 + 112 + index * 8)


def pe_bytes(data, rva, size):
    for _, section_rva, raw_size, raw_offset, _ in pe_sections(data):
        if section_rva <= rva and rva + size <= section_rva + raw_size:
            start = raw_offset + rva - section_rva
            return data[start:start + size]
    raise AssertionError(f"PE RVA {rva:#x} is not mapped")


def base_relocated(data, rva):
    directory, size = data_directory(data, 5)
    block = pe_bytes(data, directory, size)
    page = rva & ~0xFFF
    position = 0
    while position < size:
        block_page, block_size = struct.unpack_from("<II", block, position)
        if block_page == page:
            for entry in struct.unpack_from(f"<{(block_size - 8) // 2}H", block, position + 8):
                if entry >> 12 == 10 and entry & 0xFFF == rva & 0xFFF:
                    return True
        position += block_size
    return False


def first_load(image):
    offset, = struct.unpack_from("<Q", image, 32)
    size, count = struct.unpack_from("<HH", image, 54)
    return min(struct.unpack_from("<Q", image, offset + index * size + 16)[0]
               for index in range(count)
               if struct.unpack_from("<I", image, offset + index * size)[0] == 1) & ~0xFFF


def macho_bytes(data, address, size):
    for segment in MachO(data).segments:
        if segment["vmaddr"] <= address and address + size <= segment["vmaddr"] + segment["filesize"]:
            start = segment["fileoff"] + address - segment["vmaddr"]
            return data[start:start + size]
    raise AssertionError(f"Mach-O address {address:#x} is not mapped")


def read_uleb(stream, position):
    value = shift = 0
    while True:
        byte = stream[position]
        position += 1
        value |= (byte & 0x7F) << shift
        shift += 7
        if byte < 0x80:
            return value, position


def read_sleb(stream, position):
    value = shift = 0
    while True:
        byte = stream[position]
        position += 1
        value |= (byte & 0x7F) << shift
        shift += 7
        if byte < 0x80:
            return value - (1 << shift) if byte & 0x40 else value, position


def bind_symbols(data):
    macho = MachO(data)
    info = macho.dyld_info
    assert info is not None
    offset, size = info[2], info[3]
    stream = data[offset:offset + size]
    assert stream[:2] == bytes((0x3E, 0x51)), stream[:8].hex()
    position = 2
    symbols = []
    while stream[position] != 0:
        assert stream[position] in (0x40, 0x41), hex(stream[position])
        position += 1
        end = stream.index(b"\0", position)
        symbols.append(stream[position:end].decode())
        position = end + 1
        assert stream[position] == 0x60, hex(stream[position])
        _, position = read_sleb(stream, position + 1)
        assert 0x70 <= stream[position] <= 0x7F, hex(stream[position])
        _, position = read_uleb(stream, position + 1)
        assert stream[position] == 0x90, hex(stream[position])
        position += 1
    return symbols


def main():
    if sys.argv[1] == "--load":
        import ctypes
        library = ctypes.CDLL(sys.argv[2])
        function = getattr(library, "shared#guest")
        function.restype = ctypes.c_int
        assert function() == 42
        offset = ctypes.c_int64.from_address(ctypes.cast(function, ctypes.c_void_p).value + TARGET - FUNCTION).value
        thread = ctypes.c_uint64()
        libc = ctypes.CDLL(None, use_errno=True)
        assert libc.syscall(158, 0x1003, ctypes.byref(thread)) == 0, ctypes.get_errno()
        assert offset < 0, offset
        assert ctypes.string_at(thread.value + offset, 4) == bytes(range(5, 9)), (offset, ctypes.string_at(thread.value + offset, 4))
        return
    relinker = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="anyps5-guest-own-tls-") as directory:
        work = Path(directory)

        def convert(name, system, image):
            case = work / name / system
            guest = case / "sce_module" / "module.prx"
            guest.parent.mkdir(parents=True)
            guest.write_bytes(image)
            source = case / "input.elf"
            source.write_bytes(main_fixture())
            output = case / ("output.exe" if system == "windows" else "output.elf")
            flags = {"linux": [], "windows": ["--windows"], "macos": ["--macos"]}[system]
            result = subprocess.run([str(relinker), *flags, str(source), str(output)],
                                    capture_output=True, text=True, timeout=10)
            assert guest.read_bytes() == image
            return result, case / "app0" / "sce_module" / "module.prx.guest.prx", output

        for sce in (False, True):
            for info in (3, 6):
                for kind, addend in ((DTPMOD64, 0), (DTPOFF64, 4)) + (((TPOFF64, 4),) if info == 6 else ()):
                    image = fixture(sce, info=info, kind=kind, addend=addend)
                    for system in ("linux", "windows", "macos"):
                        result, artifact, output = convert(f"{sce}-{info}-{kind}", system, image)
                        assert result.returncode == 0, (sce, info, kind, system, result.stdout, result.stderr)
                        if system == "linux":
                            assert relocations(artifact.read_bytes()) == [(TARGET, kind, addend)], (sce, info, kind)
            for name, image in (("no-tls", fixture(sce, tls=False)),
                                ("local-tls-no-tls", fixture(sce, tls=False, info=6)),
                                ("valued-section", fixture(sce, value=8)),
                                ("valued-local-tls", fixture(sce, info=6, value=8)),
                                ("notype", fixture(sce, info=0)),
                                ("dtpmod-addend", fixture(sce, addend=4)),
                                ("local-tls-dtpmod-addend", fixture(sce, info=6, addend=4)),
                                ("tpoff", fixture(sce, kind=TPOFF64)),
                                ("absolute-section", fixture(sce, section=0xfff1)),
                                ("unnamed-global", fixture(sce, info=0x16)),
                                ("unnamed-weak", fixture(sce, info=0x26)),
                                ("unnamed-global-dtpoff", fixture(sce, info=0x16, kind=DTPOFF64, addend=4)),
                                ("unnamed-weak-dtpoff", fixture(sce, info=0x26, kind=DTPOFF64, addend=4)),
                                ("unnamed-global-tpoff", fixture(sce, info=0x16, kind=TPOFF64, addend=4)),
                                ("unnamed-weak-tpoff", fixture(sce, info=0x26, kind=TPOFF64, addend=4)),
                                ("suffix-only-global", fixture(sce, info=0x16, name=9)),
                                ("suffix-only-weak-dtpoff", fixture(sce, info=0x26, kind=DTPOFF64, addend=4, name=9))):
                result, artifact, output = convert(f"{sce}-{name}", "linux", image)
                assert result.returncode == 2 and "Invalid guest TLS relocation" in result.stderr, (sce, name, result.stderr)
                assert not artifact.exists() and not output.exists(), name
            for symbol, info, section in ((3, 3, 0), (0, 3, 0), (3, 6, 0), (3, 6, 1)):
                for kind, addend in ((DTPMOD64, 0), (DTPOFF64, 4), (TPOFF64, 4)):
                    if info == 3 and symbol == 3 and kind == TPOFF64:
                        continue
                    if info == 6 and section == 1:
                        image = fixture(sce, info=info, section=section, kind=kind, addend=addend)
                        for system in ("linux", "windows", "macos"):
                            result, artifact, output = convert(f"{sce}-defined-{kind}", system, image)
                            assert result.returncode == 0, (sce, kind, system, result.stdout, result.stderr)
                    image = fixture(sce, info=info, section=section, kind=kind, addend=addend, symbol=symbol, tls_memory=0)
                    for system in ("linux", "windows", "macos"):
                        name = f"{sce}-{symbol}-{info}-{section}-{kind}-empty-tls"
                        result, artifact, output = convert(name, system, image)
                        assert result.returncode == 2 and "Invalid guest TLS relocation" in result.stderr, (name, system, result.stdout, result.stderr)
                        assert not artifact.exists() and not output.exists(), (name, system)
            for section in (0, 1):
                for flags in (0x10, 0x18):
                    image = with_flags(fixture(sce, info=6, section=section, kind=TPOFF64, addend=4), flags)
                    for system in ("linux", "windows", "macos"):
                        result, artifact, output = convert(f"{sce}-static-tls-{section}-{flags}", system, image)
                        assert result.returncode == 0, (sce, section, flags, system, result.stdout, result.stderr)
                        if system == "linux":
                            assert [info & 0xffffffff for _, info, _ in relocations(artifact.read_bytes())] == [TPOFF64], (sce, section, flags)
                            if sys.platform == "linux" and os.uname().machine == "x86_64":
                                loaded = subprocess.run([sys.executable, __file__, "--load", str(artifact)], capture_output=True, text=True, timeout=10)
                                assert loaded.returncode == 0, (sce, section, flags, loaded.returncode, loaded.stderr)
            for flags in (0x1, 0x4, 0x14):
                image = with_flags(fixture(sce, info=6, section=1, kind=TPOFF64, addend=4), flags)
                result, artifact, output = convert(f"{sce}-unsupported-flags-{flags}", "linux", image)
                assert result.returncode == 2 and "Unsupported guest dynamic flags" in result.stderr, (sce, flags, result.stderr)
        tls_memory = 16
        slot = 0x10000 + TARGET - first_load(fixture(False, info=6))
        for kind, addend in ((DTPMOD64, 0), (DTPOFF64, 4), (TPOFF64, 4)):
            image = fixture(False, info=6, kind=kind, addend=addend)
            result, artifact, output = convert(f"unnamed-local-{kind}", "windows", image)
            assert result.returncode == 0, (kind, result.stdout, result.stderr)
            data = artifact.read_bytes()
            directory, size = data_directory(data, 9)
            assert directory != 0 and size >= 40, (kind, directory, size)
            index = struct.unpack("<Q", pe_bytes(data, directory + 16, 8))[0]
            expected = {DTPMOD64: index, DTPOFF64: addend, TPOFF64: addend - tls_memory}[kind]
            value = struct.unpack("<q", pe_bytes(data, slot, 8))[0]
            assert value == expected, (kind, value, expected)
            if kind == DTPMOD64:
                assert base_relocated(data, slot), slot
            result, artifact, output = convert(f"unnamed-local-{kind}", "macos", image)
            assert result.returncode == 0, (kind, result.stdout, result.stderr)
            data = artifact.read_bytes()
            descriptor = MachO(data).segment("__APS5DATA")["vmaddr"] + 0x20
            expected = {DTPMOD64: descriptor, DTPOFF64: addend, TPOFF64: addend - tls_memory}[kind]
            value = struct.unpack("<q", macho_bytes(data, 0x140000000 + slot, 8))[0]
            assert value == expected, (kind, value, expected)
            symbols = bind_symbols(data)
            assert symbols and all(symbols) and "_" not in symbols, symbols

    print("Guest own TLS relocation tests passed")


if __name__ == "__main__":
    main()
