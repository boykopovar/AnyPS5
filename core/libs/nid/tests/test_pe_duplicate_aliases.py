import struct
import subprocess
import sys
import tempfile
from pathlib import Path


def make_pe(path, function_rvas):
    binary = bytearray(0x600)
    binary[:2] = b"MZ"
    struct.pack_into("<I", binary, 0x3C, 0x80)
    binary[0x80:0x84] = b"PE\0\0"
    struct.pack_into("<HHIIIHH", binary, 0x84, 0x8664, 1, 0, 0, 0, 240, 0x2022)

    optional = 0x98
    struct.pack_into("<H", binary, optional, 0x20B)
    struct.pack_into("<I", binary, optional + 36, 0x200)
    struct.pack_into("<I", binary, optional + 108, 16)
    struct.pack_into("<II", binary, optional + 112, 0x1000, 40)

    section = optional + 240
    binary[section:section + 8] = b".edata"
    struct.pack_into("<IIII", binary, section + 8, 0x400, 0x1000, 0x400, 0x200)
    struct.pack_into("<I", binary, section + 36, 0x40000040)

    export = 0x200
    struct.pack_into(
        "<IIHHIIIIIII",
        binary,
        export,
        0,
        0,
        0,
        0,
        0,
        1,
        len(function_rvas),
        len(function_rvas),
        0x1028,
        0x1030,
        0x1038,
    )
    struct.pack_into("<II", binary, export + 40, *function_rvas)
    names = [b"LibwuIonIBw_nid_no_patch_cut", b"sceVideoOutAddVrrActiveStatusEvent"]
    cursor = 0x1040
    for index, name in enumerate(names):
        struct.pack_into("<I", binary, export + 48 + index * 4, cursor)
        struct.pack_into("<H", binary, export + 56 + index * 2, index)
        offset = 0x200 + cursor - 0x1000
        binary[offset:offset + len(name) + 1] = name + b"\0"
        cursor += len(name) + 1
    path.write_bytes(binary)


def read_names_and_targets(path):
    binary = path.read_bytes()
    export = 0x200
    function_count, name_count, functions, names, ordinals = struct.unpack_from("<IIIII", binary, export + 20)
    assert function_count == name_count == 2
    result = []
    for index in range(name_count):
        name_rva = struct.unpack_from("<I", binary, 0x200 + names - 0x1000 + index * 4)[0]
        offset = 0x200 + name_rva - 0x1000
        end = binary.index(0, offset)
        ordinal = struct.unpack_from("<H", binary, 0x200 + ordinals - 0x1000 + index * 2)[0]
        target = struct.unpack_from("<I", binary, 0x200 + functions - 0x1000 + ordinal * 4)[0]
        result.append((binary[offset:end].decode(), target))
    return result


def main():
    patcher = sys.argv[1]
    with tempfile.TemporaryDirectory() as directory:
        same_target = Path(directory) / "same-target.prx"
        make_pe(same_target, [0x2000, 0x2000])
        subprocess.run([patcher, "libSceVideoOut", str(same_target)], check=True, capture_output=True, text=True)
        exports = read_names_and_targets(same_target)
        assert exports == [("LibwuIonIBw", 0x2000), ("LibwuIonIBw", 0x2000)], exports

        different_targets = Path(directory) / "different-targets.prx"
        make_pe(different_targets, [0x2000, 0x2010])
        result = subprocess.run([patcher, "libSceVideoOut", str(different_targets)], capture_output=True, text=True)
        assert result.returncode != 0, result.stdout
        assert "duplicate patched export name: LibwuIonIBw" in result.stderr, result.stderr


if __name__ == "__main__":
    main()
