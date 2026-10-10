import pathlib
import shutil
import struct
import subprocess
import sys
import tempfile


def read_exports(path):
    data = pathlib.Path(path).read_bytes()

    def u16(offset):
        return struct.unpack_from("<H", data, offset)[0]

    def u32(offset):
        return struct.unpack_from("<I", data, offset)[0]

    pe = u32(0x3C)
    assert data[pe:pe + 4] == b"PE\0\0"
    sections = u16(pe + 6)
    optional = pe + 24
    magic = u16(optional)
    assert magic in (0x10B, 0x20B)
    directory = optional + (112 if magic == 0x20B else 96)
    export_rva = u32(directory)
    section_table = optional + u16(pe + 20)

    def offset(rva):
        for i in range(sections):
            section = section_table + i * 40
            size = max(u32(section + 8), u32(section + 16))
            start = u32(section + 12)
            if start <= rva < start + size:
                return u32(section + 20) + rva - start
        raise AssertionError(f"RVA not mapped: {rva:#x}")

    export = offset(export_rva)
    function_count = u32(export + 20)
    name_count = u32(export + 24)
    functions = offset(u32(export + 28))
    names = offset(u32(export + 32))
    ordinals = offset(u32(export + 36))

    result = []
    for i in range(name_count):
        name_offset = offset(u32(names + 4 * i))
        end = data.index(b"\0", name_offset)
        name = data[name_offset:end].decode("ascii")
        ordinal = u16(ordinals + 2 * i)
        assert ordinal < function_count
        result.append((name, u32(functions + 4 * ordinal)))

    return function_count, result



def make_conflicting_exports(path):
    data = bytearray(path.read_bytes())

    def u16(pos):
        return struct.unpack_from("<H", data, pos)[0]

    def u32(pos):
        return struct.unpack_from("<I", data, pos)[0]

    pe = u32(0x3C)
    optional = pe + 24
    directory = optional + (112 if u16(optional) == 0x20B else 96)
    sections = u16(pe + 6)
    section_table = optional + u16(pe + 20)

    def offset(rva):
        for i in range(sections):
            section = section_table + i * 40
            start = u32(section + 12)
            size = max(u32(section + 8), u32(section + 16))
            if start <= rva < start + size:
                return u32(section + 20) + rva - start
        raise AssertionError(f"RVA not mapped: {rva:#x}")

    export = offset(u32(directory))
    assert u32(export + 20) == 2
    functions = offset(u32(export + 28))

    first_rva = u32(functions)
    assert u32(functions + 4) == first_rva
    struct.pack_into("<I", data, functions + 4, first_rva + 1)
    path.write_bytes(data)


def main():
    patcher, fixture = sys.argv[1:]
    original_count, original = read_exports(fixture)

    expected_names = {
        "LibwuIonIBw_nid_no_patch_cut",
        "sceVideoOutAddVrrActiveStatusEvent",
    }
    assert original_count == 2
    assert {name for name, _ in original} == expected_names
    assert len({rva for _, rva in original}) == 1
    original_rva = original[0][1]

    with tempfile.TemporaryDirectory() as directory:
        target = pathlib.Path(directory) / pathlib.Path(fixture).name
        shutil.copyfile(fixture, target)
        subprocess.run(
            [patcher, "libSceVideoOut", str(target)],
            check=True,
        )
        patched_count, patched = read_exports(target)

    assert patched_count == 2
    assert patched == [("LibwuIonIBw", original_rva)], patched
    print("PASS: duplicate export collapsed; function RVA preserved")

    with tempfile.TemporaryDirectory() as directory:
        target = pathlib.Path(directory) / pathlib.Path(fixture).name
        shutil.copyfile(fixture, target)
        make_conflicting_exports(target)

        result = subprocess.run(
            [patcher, "libSceVideoOut", str(target)],
            capture_output=True,
            text=True,
        )

        assert result.returncode != 0, "Conflicting exports were accepted"
        assert "conflicting patched export name" in (
            result.stdout + result.stderr
        ), result.stdout + result.stderr

    print("PASS: conflicting function RVAs correctly rejected")


if __name__ == "__main__":
    main()
