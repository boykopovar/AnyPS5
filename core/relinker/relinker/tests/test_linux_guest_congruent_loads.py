from pathlib import Path
import os
import struct
import subprocess
import sys
import tempfile

from test_guest_intel_trampolines import SITE, SITE_ADDRESSES, check_trampoline, elf_bytes_at, guest_fixture, main_fixture
from test_guest_module_directories import module_with_symbol


PAGE = 0x1000
NEIGHBOUR_ERROR = "Guest PT_LOAD sharing a page with a moved load is aligned above the page"


def program_headers(data):
    offset, = struct.unpack_from("<Q", data, 32)
    size, count = struct.unpack_from("<HH", data, 54)
    return [struct.unpack_from("<IIQQQQQQ", data, offset + index * size) for index in range(count)]


def loads(data):
    return [header for header in program_headers(data) if header[0] == 1]


def module(code=(0x400, 0x100), data=(0x600, 0x100), extra=()):
    original = module_with_symbol(True)
    original[0x400:0x406] = bytes.fromhex("b8 2a 00 00 00 c3")
    struct.pack_into("<Q", original, 0x898 + 16, 6)
    headers = [list(struct.unpack_from("<IIQQQQQQ", original, 64 + index * 56)) for index in range(3)]
    sources = [original[0x400:0x500], original[0x600:0xA00]]
    placed = [(code, sources[0]), (data, sources[1])]
    ends = [offset + len(source) for (offset, _), source in placed]
    ends += [offset + file_size for _, offset, _, file_size, _, _, _ in extra]
    image = bytearray(max(ends))
    image[:64] = original[:64]
    image[0x200:] = b"\x33" * (len(image) - 0x200)
    for index, ((offset, align), source) in enumerate(placed):
        image[offset:offset + len(source)] = source
        headers[index][2] = offset
        headers[index][7] = align
    headers[2][2] = data[0]
    packed = [struct.pack("<IIQQQQQQ", *header) for header in headers]
    for flags, offset, address, file_size, memory_size, align, fill in extra:
        image[offset:offset + file_size] = bytes((fill,)) * file_size
        packed.append(struct.pack("<IIQQQQQQ", 1, flags, offset, address, address, file_size, memory_size, align))
    image[64:64 + len(packed) * 56] = b"".join(packed)
    struct.pack_into("<H", image, 56, len(packed))
    return image


def mapped_memory(data):
    memory = bytearray(0x10000)
    for _, _, offset, address, _, file_size, memory_size, _ in loads(data):
        assert offset % PAGE == address % PAGE, (hex(offset), hex(address))
        start = address - address % PAGE
        end = page_end(address + file_size)
        source = offset - address % PAGE
        page = data[source:source + end - start]
        memory[start:end] = page + bytes(end - start - len(page))
        if memory_size > file_size:
            zero = address + file_size
            end = max(address + memory_size, page_end(zero))
            memory[zero:end] = bytes(end - zero)
    return memory


def page_end(address):
    return -(-address // PAGE) * PAGE


def check_mapping(name, image, data):
    memory = mapped_memory(data)
    for _, _, offset, address, _, file_size, memory_size, _ in loads(image):
        assert memory[address:address + file_size] == image[offset:offset + file_size], (name, hex(address))
        assert memory[address + file_size:address + memory_size] == bytes(memory_size - file_size), (name, hex(address))
    output = {header[3]: header for header in loads(data)}
    for _, _, offset, address, _, file_size, _, align in loads(image):
        moved = output[address]
        assert data[moved[2]:moved[2] + file_size] == image[offset:offset + file_size], (name, hex(address))
        assert moved[7] == align, (name, hex(address), moved[7], align)


def convert(relinker, case, image, options=()):
    modules = case / "sce_module"
    modules.mkdir(parents=True)
    source = case / "input.elf"
    source.write_bytes(main_fixture())
    (modules / "loads.prx").write_bytes(image)
    result = subprocess.run([str(relinker), *options, str(source), str(case / "output.elf")],
                            capture_output=True, text=True, timeout=10)
    artifact = case / "app0/sce_module/loads.prx.guest.prx"
    return result, artifact


def call(artifact):
    if sys.platform != "linux" or os.uname().machine != "x86_64":
        return
    code = "import ctypes, sys; sys.exit(getattr(ctypes.CDLL(sys.argv[1]), 'shared#guest')())"
    loaded = subprocess.run([sys.executable, "-c", code, str(artifact)], capture_output=True, text=True, timeout=10)
    assert loaded.returncode == 42, (artifact, loaded.returncode, loaded.stderr)


CASES = (
    ("unaligned", module()),
    ("align-0", module((0x400, 0), (0x600, 0))),
    ("align-1", module((0x400, 1), (0x600, 1))),
    ("align-10", module((0x400, 0x10), (0x600, 0x10))),
    ("lower-neighbour", module((0x1000, 0x1000), (0x2000, 0x1000),
                               ((4, 0x3000, 0x3000, 0x80, 0x80, 0x10, 0x11), (4, 0x3810, 0x3100, 0x80, 0x80, 0x10, 0x22)))),
    ("upper-neighbour", module((0x1000, 0x1000), (0x2000, 0x1000),
                               ((4, 0x3810, 0x3000, 0x80, 0x80, 0x10, 0x11), (4, 0x3100, 0x3100, 0x80, 0x80, 0x10, 0x22)))),
    ("no-file-bytes", module(extra=((6, 0xA10, 0x5000, 0, 0x80, 0x10, 0), (6, 0xA10, 0x6020, 0, 0, 0x10, 0)))),
    ("no-file-bytes-neighbour", module((0x1000, 0x1000), (0x2000, 0x1000),
                                       ((4, 0x3810, 0x3000, 0x80, 0x80, 0x10, 0x11), (6, 0x3100, 0x3100, 0, 0x80, 0x10, 0)))),
)


def main():
    relinker = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="anyps5-guest-congruent-") as directory:
        work = Path(directory)
        for name, image in CASES:
            result, artifact = convert(relinker, work / name, image)
            assert result.returncode == 0, (name, result.stderr)
            check_mapping(name, image, artifact.read_bytes())
            call(artifact)
        aligned = module((0x1000, 0x1000), (0x2000, 0x1000))
        result, artifact = convert(relinker, work / "aligned", aligned)
        assert result.returncode == 0, result.stderr
        data = artifact.read_bytes()
        assert [header[2] for header in loads(data)[:2]] == [0x1000, 0x2000], loads(data)
        check_mapping("aligned", aligned, data)
        call(artifact)
        large = module((0x1000, 0x1000), (0x2000, 0x1000),
                       ((4, 0x3810, 0x3000, 0x80, 0x80, 0x10, 0x11), (4, 0x3100, 0x3100, 0x80, 0x80, 0x4000, 0x22)))
        same = bytearray(large)
        same[0x3000:0x3080] = bytes((0x11,)) * 0x80
        result, artifact = convert(relinker, work / "large-neighbour-same-page", same)
        assert result.returncode == 0, result.stderr
        data = artifact.read_bytes()
        assert [header[2] for header in loads(data) if header[3] == 0x3100] == [0x3100], loads(data)
        check_mapping("large-neighbour-same-page", same, data)
        call(artifact)
        result, artifact = convert(relinker, work / "large-neighbour", large)
        assert result.returncode == 2 and NEIGHBOUR_ERROR in result.stderr, (result.returncode, result.stderr)
        assert not artifact.exists()
        result, artifact = convert(relinker, work / "trampoline", guest_fixture(SITE), ("--to-intel",))
        assert result.returncode == 0, result.stderr
        data = artifact.read_bytes()
        for _, _, offset, address, _, file_size, _, _ in loads(data):
            assert offset % PAGE == address % PAGE, (hex(offset), hex(address))
        for site_address in SITE_ADDRESSES:
            check_trampoline(lambda address, size: elf_bytes_at(data, loads(data), address, size), site_address)
    print("Linux guest congruent load tests passed")


if __name__ == "__main__":
    main()
