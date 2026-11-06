"""Validate the --icon resource directory through the real ELF-to-PE pipeline."""

from pathlib import Path
import struct
import subprocess
import sys
import tempfile

from test_optional_plt import fixture

RT_ICON = 3
RT_GROUP_ICON = 14


def icon(images):
    header = struct.pack("<HHH", 0, 1, len(images))
    offset = len(header) + 16 * len(images)
    entries = b""
    payloads = b""
    for width, payload in images:
        entries += struct.pack("<BBBBHHII", width, width, 0, 0, 1, 32, len(payload), offset)
        payloads += payload
        offset += len(payload)
    return header + entries + payloads


def sections(pe):
    pe_offset = struct.unpack_from("<I", pe, 0x3c)[0]
    count = struct.unpack_from("<H", pe, pe_offset + 6)[0]
    optional = struct.unpack_from("<H", pe, pe_offset + 20)[0]
    table = pe_offset + 24 + optional
    result = []
    for index in range(count):
        header = table + index * 40
        name = pe[header:header + 8].rstrip(b"\0").decode()
        virtual_size, rva, raw_size, raw_offset = struct.unpack_from("<IIII", pe, header + 8)
        result.append((name, rva, virtual_size, raw_offset, raw_size))
    return result


def directory(pe, index):
    pe_offset = struct.unpack_from("<I", pe, 0x3c)[0]
    return struct.unpack_from("<II", pe, pe_offset + 24 + 112 + index * 8)


def read_rva(pe, rva, size):
    for _, section_rva, virtual_size, raw_offset, _ in sections(pe):
        if section_rva <= rva < section_rva + virtual_size:
            start = raw_offset + rva - section_rva
            return pe[start:start + size]
    raise AssertionError(f"RVA {rva:#x} is not inside any section")


def entries(resources, offset):
    named, ids = struct.unpack_from("<HH", resources, offset + 12)
    result = []
    for index in range(named + ids):
        entry = offset + 16 + index * 8
        identifier, target = struct.unpack_from("<II", resources, entry)
        result.append((identifier, target & 0x7fffffff, bool(target & 0x80000000)))
    return result


def leaves(pe, resources, base_rva, type_id):
    for identifier, target, is_directory in entries(resources, 0):
        if identifier != type_id:
            continue
        assert is_directory, "resource type entry must point at a subdirectory"
        result = {}
        for name, name_target, name_is_directory in entries(resources, target):
            assert name_is_directory, "resource name entry must point at a subdirectory"
            languages = entries(resources, name_target)
            assert len(languages) == 1 and not languages[0][2], "expected a single language leaf"
            data_rva, data_size = struct.unpack_from("<II", resources, languages[0][1])
            assert base_rva <= data_rva, "resource data must live inside the resource section"
            result[name] = read_rva(pe, data_rva, data_size)
        return result
    raise AssertionError(f"resource type {type_id} is missing")


def main():
    relinker = Path(sys.argv[1]).resolve()
    images = [(16, b"\xa5" * 64), (32, b"\x5a" * 128), (48, b"\x42" * 96)]
    with tempfile.TemporaryDirectory(prefix="anyps5-icon-") as directory_name:
        work = Path(directory_name)
        source = work / "input.elf"
        source.write_bytes(fixture())
        icon_path = work / "app.ico"
        icon_path.write_bytes(icon(images))

        plain = work / "plain.exe"
        result = subprocess.run([str(relinker), "--skip-sce-module", "--windows", str(source), str(plain)],
                                capture_output=True, text=True, timeout=20)
        assert result.returncode == 0, (result.stdout, result.stderr)
        assert directory(plain.read_bytes(), 2) == (0, 0), "resource directory must stay empty without --icon"
        assert not any(name == ".rsrc" for name, *_ in sections(plain.read_bytes())), "unexpected .rsrc section"

        output = work / "icon.exe"
        result = subprocess.run([str(relinker), "--skip-sce-module", "--windows", "--icon", str(icon_path),
                                 str(source), str(output)], capture_output=True, text=True, timeout=20)
        assert result.returncode == 0, (result.stdout, result.stderr)
        pe = output.read_bytes()

        rsrc = [section for section in sections(pe) if section[0] == ".rsrc"]
        assert len(rsrc) == 1, "expected exactly one .rsrc section"
        _, rsrc_rva, rsrc_size, rsrc_offset, _ = rsrc[0]
        assert directory(pe, 2) == (rsrc_rva, rsrc_size), "resource data directory does not describe .rsrc"
        resources = pe[rsrc_offset:rsrc_offset + rsrc_size]

        icons = leaves(pe, resources, rsrc_rva, RT_ICON)
        assert sorted(icons) == [1, 2, 3], f"unexpected RT_ICON ids: {sorted(icons)}"
        for index, (_, payload) in enumerate(images):
            assert icons[index + 1] == payload, f"RT_ICON {index + 1} payload does not match the source image"

        groups = leaves(pe, resources, rsrc_rva, RT_GROUP_ICON)
        assert list(groups) == [1], f"unexpected RT_GROUP_ICON ids: {list(groups)}"
        group = groups[1]
        reserved, kind, count = struct.unpack_from("<HHH", group, 0)
        assert (reserved, kind, count) == (0, 1, len(images)), "malformed GRPICONDIR header"
        assert len(group) == 6 + 14 * len(images), "malformed GRPICONDIR size"
        for index, (width, payload) in enumerate(images):
            entry = 6 + index * 14
            assert group[entry] == width and group[entry + 1] == width, "GRPICONDIR entry has the wrong size"
            assert struct.unpack_from("<I", group, entry + 8)[0] == len(payload), "GRPICONDIR entry has the wrong length"
            assert struct.unpack_from("<H", group, entry + 12)[0] == index + 1, "GRPICONDIR entry points at the wrong RT_ICON"

        broken = work / "broken.ico"
        broken.write_bytes(b"\x00\x00\x02\x00\x01\x00" + b"\x00" * 16)
        rejected = work / "rejected.exe"
        result = subprocess.run([str(relinker), "--skip-sce-module", "--windows", "--icon", str(broken),
                                 str(source), str(rejected)], capture_output=True, text=True, timeout=20)
        assert result.returncode != 0 and not rejected.exists(), "a cursor file was accepted as an icon"

        missing = work / "missing.exe"
        result = subprocess.run([str(relinker), "--skip-sce-module", "--icon", str(icon_path), str(source), str(missing)],
                                capture_output=True, text=True, timeout=20)
        assert result.returncode != 0 and not missing.exists(), "--icon was accepted without --windows"
    print("Windows icon resource integration tests passed")


if __name__ == "__main__":
    main()
