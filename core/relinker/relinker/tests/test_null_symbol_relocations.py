import json
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

from test_dynamic_segment import fixture
from test_linux_dynamic_strings import dynamic_strings, file_offset
from test_linux_load_alignment import loads


GLOB_DAT_SLOT = 0x308
ABSOLUTE_SLOT = 0x310
ADDEND = 0x1234
DEFAULT_BASE = 0x400000
CHECK_SLOTS = bytes.fromhex(
    "488b0501010000" "4885c0" "751a"
    "488b0dfd000000" "4881f934120000" "750a"
    "b82a000000c3" "cccccccc" "b807000000c3")
TARGETS = (("linux", [], b"\x7fELF"), ("windows", ["--windows"], b"MZ"), ("macos", ["--macos"], b"\xcf\xfa\xed\xfe"))


def image_with(symbol=b"", index=1):
    image = fixture()
    relocations = [(GLOB_DAT_SLOT, (index << 32) | 6, 0), (ABSOLUTE_SLOT, (index << 32) | 1, ADDEND)]
    struct.pack_into("<Q", image, 0x458, len(relocations) * 24)
    for index, relocation in enumerate(relocations):
        struct.pack_into("<QQq", image, 0x700 + index * 24, *relocation)
    image[0x200:0x200 + len(CHECK_SLOTS)] = CHECK_SLOTS
    struct.pack_into("<QQ", image, GLOB_DAT_SLOT, 0x5A5A5A5A5A5A5A5A, 0x5A5A5A5A5A5A5A5A)
    image[0x638:0x638 + len(symbol)] = symbol
    struct.pack_into("<qQ", image, 0x470, 0x6100003f, 48)
    struct.pack_into("<QQ", image, 152, 0x90, 0x90)
    return image


def linux_imports(data):
    tags, strings = dynamic_strings(data)
    segments = loads(data)
    symbols = file_offset(data, segments, tags[6])
    relocations = file_offset(data, segments, tags[7])
    imports = []
    for offset in range(relocations, relocations + tags[8], 24):
        target, info, addend = struct.unpack_from("<QQq", data, offset)
        if info & 0xffffffff == 8:
            continue
        name, bind_type, _, section, value, size = struct.unpack_from("<IBBHQQ", data, symbols + (info >> 32) * 24)
        imports.append((target, info & 0xffffffff, addend, strings[name:strings.index(b"\0", name)], bind_type, section, value, size))
    return sorted(imports)


def main():
    relinker = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="anyps5-null-symbol-") as directory:
        work = Path(directory)
        for index in (1, 0):
            source = work / f"null-entry-{index}.elf"
            source.write_bytes(image_with(index=index))
            for target, options, magic in TARGETS:
                for level in (0, 1):
                    output = work / f"null-entry-{index}-{target}-{level}.out"
                    result = subprocess.run([str(relinker), "--skip-sce-module", "--registry", f"unused-filter={level}",
                                             *options, str(source), str(output)], capture_output=True, text=True, timeout=20)
                    assert result.returncode == 0, (index, target, level, result.stdout, result.stderr)
                    data = output.read_bytes()
                    assert data.startswith(magic), (index, target)
                    registry = json.loads(output.with_suffix(".registry.json").read_text(encoding="utf-8"))
                    assert registry == [], (index, target, level, registry)
                    if target == "linux":
                        assert linux_imports(data) == [(DEFAULT_BASE + GLOB_DAT_SLOT, 6, 0, b"", 0x20, 0, 0, 0),
                                                       (DEFAULT_BASE + ABSOLUTE_SLOT, 1, ADDEND, b"", 0x20, 0, 0, 0)], (index, level)
                    if target == "windows" and os.name == "nt":
                        run = subprocess.run([str(output)], capture_output=True, timeout=20)
                        assert run.returncode == 42, (index, level, run.returncode, run.stderr)
        source = work / "named-unnamed.elf"
        source.write_bytes(image_with(struct.pack("<IBBHQQ", 0, 0x12, 0, 0, 0, 0)))
        for target, options, _ in TARGETS:
            output = work / f"named-unnamed-{target}.out"
            result = subprocess.run([str(relinker), "--skip-sce-module", *options, str(source), str(output)],
                                    capture_output=True, text=True, timeout=20)
            assert result.returncode == 2 and "Unnamed import symbol is not a null symbol entry" in result.stderr, (target, result.stderr)
            assert not output.exists()
    print("Null symbol relocation tests passed")


if __name__ == "__main__":
    main()
