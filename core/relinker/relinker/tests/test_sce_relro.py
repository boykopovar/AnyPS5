import json
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

from test_linux_dynamic_strings import dynamic_strings, file_offset
from test_linux_load_alignment import loads

PT_LOAD = 1
PT_DYNAMIC = 2
PT_SCE_RELRO = 0x61000010
DEFAULT_BASE = 0x400000
PT_SCE_VERSION = 0x6FFFFF01


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


def main():
    relinker = Path(sys.argv[1]).resolve()
    standalone = [(PT_SCE_RELRO, 4, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000)]
    covered = [(PT_LOAD, 6, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000), (PT_SCE_RELRO, 4, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000)]
    with tempfile.TemporaryDirectory(prefix="anyps5-sce-relro-") as directory:
        work = Path(directory)
        for name, extra in (("standalone", standalone), ("covered", covered)):
            for target, options in (("linux", []), ("windows", ["--windows"]), ("macos", ["--macos"])):
                result, output = relink(relinker, work, f"{name}-{target}", fixture(extra), options)
                assert result.returncode == 0, (name, target, result.stdout, result.stderr)
                if target == "linux":
                    data = output.read_bytes()
                    relro = [load for load in loads(data) if load[3] <= DEFAULT_BASE + 0x1000 < load[3] + load[6]]
                    assert len(relro) == 1 and relro[0][1] == 6 and relro[0][3] == DEFAULT_BASE + 0x1000 and relro[0][5:7] == (0x1000, 0x1000), (name, relro)
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
                result, output = relink(relinker, work, f"partial-{name}", fixture([load, standalone[0]]), options)
                assert result.returncode == 2 and "PT_SCE_RELRO partially overlaps a PT_LOAD segment" in result.stderr, (name, result.stderr)
                assert not output.exists()
    print("PT_SCE_RELRO tests passed")


if __name__ == "__main__":
    main()
