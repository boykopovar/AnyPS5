import json
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

from test_guest_module_directories import module_with_symbol
from test_guest_needed_modules import executable_with_needed
from test_linux_dynamic_strings import dynamic_strings, file_offset
from test_linux_load_alignment import loads

PT_LOAD = 1
PT_DYNAMIC = 2
PT_SCE_RELRO = 0x61000010
PT_GNU_RELRO = 0x6474E552
DEFAULT_BASE = 0x400000
PT_SCE_VERSION = 0x6FFFFF01
ACCESS_VIOLATION = 0xC0000005
STANDALONE = [(PT_SCE_RELRO, 4, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000)]
COVERED = [(PT_LOAD, 6, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000), (PT_SCE_RELRO, 4, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000)]


def gnu_relro_headers(elf):
    phoff, = struct.unpack_from("<Q", elf, 0x20)
    phentsize, phnum = struct.unpack_from("<HH", elf, 0x36)
    headers = [struct.unpack_from("<IIQQQQQQ", elf, phoff + index * phentsize) for index in range(phnum)]
    return [header for header in headers if header[0] == PT_GNU_RELRO]


def fixture(extra):
    image = bytearray(0x3000)
    image[:16] = b"\x7fELF\x02\x01\x01" + bytes(9)
    headers = [(PT_LOAD, 5, 0, 0, 0x1000, 0x1000, 0x1000), *extra]
    tags = [(5, 0x600), (10, 1), (6, 0x620), (11, 24), (7, 0x700), (8, 24), (9, 24), (0, 0)]
    headers.append((PT_DYNAMIC, 6, 0x400, 0x400, len(tags) * 16, len(tags) * 16, 8))
    headers += [(PT_SCE_VERSION, 0, 0, 0, 0, 0, 1)] * (7 - len(headers))
    struct.pack_into("<HHIQQQIHHHHHH", image, 16, 3, 62, 1, 0x200, 64, 0, 0, 64, 56, len(headers), 64, 0, 0)
    for index, (kind, flags, offset, address, file_size, memory_size, align) in enumerate(headers):
        struct.pack_into("<IIQQQQQQ", image, 64 + index * 56, kind, flags, offset, address, address, file_size, memory_size, align)
    image[0x200:0x206] = b"\xff\x25\xfa\x0d\x00\x00"
    image[0x210:0x216] = b"\xb8\x2a\x00\x00\x00\xc3"
    for index, tag in enumerate(tags):
        struct.pack_into("<qQ", image, 0x400 + index * 16, *tag)
    struct.pack_into("<QQq", image, 0x700, 0x1000, 8, 0x210)
    return image


def relink(relinker, work, name, image, options):
    source = work / f"{name}.elf"
    output = work / f"{name}.out"
    source.write_bytes(image)
    result = subprocess.run([str(relinker), "--skip-sce-module", *options, str(source), str(output)],
                            capture_output=True, text=True, timeout=20)
    return result, output


def filtered_fixture(extra, lifecycle):
    image = fixture(extra)
    if not lifecycle:
        image[0x1100:0x1118] = image[0x700:0x718]
        struct.pack_into("<Q", image, 0x400 + 4 * 16 + 8, 0x1100)
        struct.pack_into("<qQqQ", image, 0x470, 0x6100003f, 24, 0, 0)
        struct.pack_into("<QQ", image, 64 + (1 + len(extra)) * 56 + 32, 9 * 16, 9 * 16)
        return image
    image[0x200] = 0xc3
    image[0x210:0x218] = b"\x48\x8b\x05" + struct.pack("<i", 0x1300 - 0x217) + b"\xc3"
    image[0x600:0x605] = b"\0foo\0"
    struct.pack_into("<IBBHQQ", image, 0x638, 1, 0x12, 0, 0, 0, 0)
    struct.pack_into("<QQq", image, 0x700, 0x1200, 8, 0x210)
    struct.pack_into("<QQq", image, 0x718, 0x1300, (1 << 32) | 6, 0)
    tags = [(5, 0x600), (10, 5), (6, 0x620), (11, 24), (7, 0x700), (8, 48), (9, 24),
            (25, 0x1200), (27, 8), (0x6100003f, 48), (0, 0)]
    dynamic = 64 + (1 + len(extra)) * 56
    struct.pack_into("<QQ", image, dynamic + 32, len(tags) * 16, len(tags) * 16)
    for index, tag in enumerate(tags):
        struct.pack_into("<qQ", image, 0x400 + index * 16, *tag)
    return image


def storing_fixture(extra, address):
    image = fixture(extra)
    image[0x200:0x207] = b"\xc6\x05" + struct.pack("<i", address - 0x207) + b"\x01"
    image[0x207:0x20d] = b"\xff\x25" + struct.pack("<i", 0x1000 - 0x20d)
    return image


def storing_guest(kind):
    image = module_with_symbol(True)
    image.extend(bytes(0xC00 - len(image)))
    struct.pack_into("<IIQQQQQQ", image, 232, kind, 4 if kind == PT_SCE_RELRO else 6, 0, 0x3000, 0x3000, 0, 0x1000, 0x1000)
    struct.pack_into("<H", image, 56, 4)
    image[0x480:0x488] = b"\xc6\x05" + struct.pack("<i", 0x3008 - 0x1087) + b"\x01\xc3"
    tags = []
    for offset in range(0x600, 0x700, 16):
        tag, value = struct.unpack_from("<qQ", image, offset)
        if tag == 0:
            break
        tags.append((tag, value))
    tags += [(12, 0x1080), (0, 0)]
    for index, tag in enumerate(tags):
        struct.pack_into("<qQ", image, 0x600 + index * 16, *tag)
    struct.pack_into("<QQ", image, 176 + 32, len(tags) * 16, len(tags) * 16)
    return image


def check_windows_stores(relinker, work):
    writable = (PT_LOAD, 6, 0x2000, 0x2000, 0x1000, 0x1000, 0x1000)
    for name, extra, address, expected in (("relro", [writable, *COVERED], 0x1008, ACCESS_VIOLATION),
                                           ("writable", [writable, *COVERED], 0x2008, 42),
                                           ("standalone", STANDALONE, 0x1008, ACCESS_VIOLATION),
                                           ("earlier", [writable, (PT_SCE_RELRO, 4, 0x2000, 0x2000, 0x1000, 0x1000, 0x1000), *COVERED], 0x2008, 42),
                                           ("last", [writable, (PT_SCE_RELRO, 4, 0x2000, 0x2000, 0x1000, 0x1000, 0x1000), *COVERED], 0x1008, ACCESS_VIOLATION)):
        result, output = relink(relinker, work, f"store-{name}", storing_fixture(extra, address), ["--windows"])
        assert result.returncode == 0, (name, result.stdout, result.stderr)
        run = subprocess.run([str(output)], capture_output=True, timeout=20)
        assert run.returncode == expected, (name, hex(run.returncode), run.stderr)
    for kind, expected in ((PT_SCE_RELRO, ACCESS_VIOLATION), (PT_LOAD, 42)):
        case = work / f"guest-store-{kind:x}"
        (case / "sce_module").mkdir(parents=True)
        (case / "sce_module" / "needed.prx").write_bytes(storing_guest(kind))
        source = case / "input.elf"
        source.write_bytes(executable_with_needed())
        output = case / "output.exe"
        result = subprocess.run([str(relinker), "--windows", str(source), str(output)], capture_output=True, text=True, timeout=20)
        assert result.returncode == 0, (kind, result.stdout, result.stderr)
        run = subprocess.run([str(output)], capture_output=True, timeout=20)
        assert run.returncode == expected, (kind, hex(run.returncode), run.stderr)


def main():
    relinker = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="anyps5-sce-relro-") as directory:
        work = Path(directory)
        if os.name == "nt":
            check_windows_stores(relinker, work)
        for name, extra in (("standalone", STANDALONE), ("covered", COVERED)):
            for target, options in (("linux", []), ("windows", ["--windows"]), ("macos", ["--macos"])):
                result, output = relink(relinker, work, f"{name}-{target}", fixture(extra), options)
                assert result.returncode == 0, (name, target, result.stdout, result.stderr)
                if target == "linux":
                    data = output.read_bytes()
                    relro = [load for load in loads(data) if load[3] <= DEFAULT_BASE + 0x1000 < load[3] + load[6]]
                    assert len(relro) == 1 and relro[0][1] == 6 and relro[0][3] == DEFAULT_BASE + 0x1000 and relro[0][5:7] == (0x1000, 0x1000), (name, relro)
                    gnu_relro = gnu_relro_headers(data)
                    assert len(gnu_relro) == 1 and gnu_relro[0][1] == 4 and gnu_relro[0][3] == DEFAULT_BASE + 0x1000 and gnu_relro[0][5:7] == (0x1000, 0x1000), (name, gnu_relro)
                    tags, _ = dynamic_strings(data)
                    start = file_offset(data, loads(data), tags[7])
                    assert [struct.unpack_from("<QQq", data, offset) for offset in range(start, start + tags[8], 24)] == [(DEFAULT_BASE + 0x1000, 8, DEFAULT_BASE + 0x210)], name
                if target == "windows" and os.name == "nt":
                    run = subprocess.run([str(output)], capture_output=True, timeout=20)
                    assert run.returncode == 42, (name, run.returncode, run.stderr)
            for lifecycle in (False, True):
                for level in (0, 1, 2):
                    case = f"{name}-filter-{level}-{'lifecycle' if lifecycle else 'relocations'}"
                    result, output = relink(relinker, work, case, filtered_fixture(extra, lifecycle),
                                            ["--registry", f"unused-filter={level}"])
                    assert result.returncode == 0, (case, result.stdout, result.stderr)
                    registry = json.loads(output.with_suffix(".registry.json").read_text())
                    assert [entry["nid"] for entry in registry] == (["foo"] if lifecycle else []), (case, registry)
        for name, load in (("tail", (PT_LOAD, 6, 0x1000, 0x1000, 0x800, 0x800, 0x1000)),
                           ("head", (PT_LOAD, 6, 0x1800, 0x1800, 0x800, 0x800, 0x1000))):
            for options in ([], ["--windows"]):
                result, output = relink(relinker, work, f"partial-{name}", fixture([load, STANDALONE[0]]), options)
                assert result.returncode == 2 and "PT_SCE_RELRO partially overlaps a PT_LOAD segment" in result.stderr, (name, result.stderr)
                assert not output.exists()
    print("PT_SCE_RELRO tests passed")


if __name__ == "__main__":
    main()
