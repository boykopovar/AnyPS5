"""Check that a title bundling libc.prx still lists the host libc.prx, which the host PRX libraries need."""

from pathlib import Path
import struct
import subprocess
import sys
import tempfile

from test_guest_intel_trampolines import PLAIN_SITE, guest_fixture, main_fixture

DYNAMIC_OFFSET = 0x4800
STRINGS_OFFSET = 0x4a00


def main_importing(*libraries):
    image = main_fixture()
    strings = b"\0" + b"".join(name.encode() + b"\0" for name in libraries)
    image[STRINGS_OFFSET:STRINGS_OFFSET + len(strings)] = strings
    offset = 1
    tags = []
    for name in libraries:
        tags.append((1, offset))
        offset += len(name) + 1
    tags += [(5, STRINGS_OFFSET), (10, len(strings)), (6, 0x4620), (11, 24), (7, 0x4700), (8, 0), (9, 24), (0, 0)]
    for index, tag in enumerate(tags):
        struct.pack_into("<qQ", image, DYNAMIC_OFFSET + index * 16, *tag)
    struct.pack_into("<QQQQQ", image, 120 + 8, DYNAMIC_OFFSET, DYNAMIC_OFFSET, DYNAMIC_OFFSET, len(tags) * 16, len(tags) * 16)
    return image


def needed(data):
    table, = struct.unpack_from("<Q", data, 32)
    size, count = struct.unpack_from("<HH", data, 54)
    headers = [struct.unpack_from("<IIQQQQQQ", data, table + index * size) for index in range(count)]
    loads = [header for header in headers if header[0] == 1]

    def file_offset(address):
        for _, _, offset, mapped, _, file_size, _, _ in loads:
            if mapped <= address < mapped + file_size:
                return offset + address - mapped
        raise AssertionError(f"Unmapped address {address:#x}")

    _, _, offset, _, _, size, _, _ = next(header for header in headers if header[0] == 2)
    entries = [struct.unpack_from("<qQ", data, offset + position) for position in range(0, size, 16)]
    strings = file_offset(next(value for tag, value in entries if tag == 5))
    names = []
    for tag, value in entries:
        if tag == 1:
            start = strings + value
            names.append(data[start:data.index(b"\0", start)].decode())
    return names


def relink(relinker, directory, modules):
    case = Path(directory)
    module_dir = case / "sce_module"
    module_dir.mkdir()
    source = case / "input.elf"
    source.write_bytes(main_importing("libkernel.prx", "libSceLibcInternal.prx"))
    for name in modules:
        (module_dir / name).write_bytes(guest_fixture(PLAIN_SITE))
    result = subprocess.run([str(relinker), str(source), str(case / "output.elf")],
                            capture_output=True, text=True, timeout=30)
    assert result.returncode == 0, (result.stdout, result.stderr)
    return needed((case / "output.elf").read_bytes())


def main():
    relinker = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="anyps5-guest-libc-") as directory:
        work = Path(directory)
        for name in ("bundled", "other"):
            (work / name).mkdir()
        bundled = relink(relinker, work / "bundled", ["libc.prx"])
        assert bundled[-3:] == ["libkernel.prx", "libSceLibcInternal.prx", "libc.prx"], bundled
        assert bundled.count("libc.prx") == 1, bundled
        other = relink(relinker, work / "other", ["sample.prx"])
        assert "libc.prx" not in other, other
    print("Guest libc integration tests passed")


if __name__ == "__main__":
    main()
