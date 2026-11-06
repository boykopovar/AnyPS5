from pathlib import Path
import ctypes
import os
import struct
import subprocess
import sys
import tempfile
import zlib

from test_optional_plt import fixture


def chunk(kind, data):
    return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))


def png(width, height):
    rows = b"".join(b"\0" + bytes((15, 65, 200, 255)) * width for _ in range(height))
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)) +
            chunk(b"IDAT", zlib.compress(rows)) + chunk(b"IEND", b""))


def resources(pe):
    header = struct.unpack_from("<I", pe, 0x3c)[0]
    optional = header + 24
    rva, size = struct.unpack_from("<II", pe, optional + 112 + 2 * 8)
    count = struct.unpack_from("<H", pe, header + 6)[0]
    table = optional + struct.unpack_from("<H", pe, header + 20)[0]
    sections = []
    for index in range(count):
        offset = table + index * 40
        name = pe[offset:offset + 8].rstrip(b"\0")
        virtual_size, address, raw_size, raw = struct.unpack_from("<IIII", pe, offset + 8)
        flags = struct.unpack_from("<I", pe, offset + 36)[0]
        sections.append((name, virtual_size, address, raw_size, raw, flags))
    if rva == 0:
        assert size == 0 and all(section[0] != b".rsrc" for section in sections)
        return None
    matches = [section for section in sections if section[0] == b".rsrc"]
    assert len(matches) == 1, sections
    _, virtual_size, address, raw_size, raw, flags = matches[0]
    assert rva == address and size == virtual_size <= raw_size
    assert rva % 0x1000 == 0 and raw % 0x200 == 0 and raw_size % 0x200 == 0
    assert flags == 0x40000040 and raw + raw_size <= len(pe)
    assert struct.unpack_from("<I", pe, optional + 56)[0] >= rva + size
    data = pe[raw:raw + size]

    def directory(offset):
        assert offset % 4 == 0 and offset + 16 <= size
        named, ids = struct.unpack_from("<HH", data, offset + 12)
        assert named == 0 and offset + 16 + ids * 8 <= size
        entries = dict(struct.unpack_from("<II", data, offset + 16 + index * 8) for index in range(ids))
        assert len(entries) == ids
        return entries

    root = directory(0)
    assert set(root) == {3, 14}, root
    result = {}
    for kind, target in root.items():
        assert target & 0x80000000
        names = directory(target & 0x7fffffff)
        assert set(names) == {1} and names[1] & 0x80000000
        languages = directory(names[1] & 0x7fffffff)
        assert set(languages) == {0}
        leaf = languages[0]
        assert not leaf & 0x80000000 and leaf % 4 == 0 and leaf + 16 <= size
        data_rva, length, codepage, reserved = struct.unpack_from("<IIII", data, leaf)
        assert codepage == reserved == 0 and data_rva % 4 == 0
        offset = data_rva - rva
        assert 0 <= offset <= size and length <= size - offset
        result[kind] = data[offset:offset + length]
    return result


def check_extraction(path):
    if os.name != "nt":
        return
    shell = ctypes.WinDLL("shell32")
    user = ctypes.WinDLL("user32")
    shell.ExtractIconExW.argtypes = [ctypes.c_wchar_p, ctypes.c_int, ctypes.POINTER(ctypes.c_void_p),
                                  ctypes.POINTER(ctypes.c_void_p), ctypes.c_uint]
    shell.ExtractIconExW.restype = ctypes.c_uint
    user.DestroyIcon.argtypes = [ctypes.c_void_p]
    large, small = ctypes.c_void_p(), ctypes.c_void_p()
    try:
        count = shell.ExtractIconExW(str(path), 0, ctypes.byref(large), ctypes.byref(small), 1)
        assert count > 0 and large.value and small.value, "Windows could not extract the icon"
    finally:
        for handle in (large.value, small.value):
            if handle:
                user.DestroyIcon(handle)


def main():
    relinker = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="anyps5-icon-") as directory:
        work = Path(directory)
        app = work / "app with spaces"
        app.mkdir()
        source = app / "eboot.bin"
        source.write_bytes(fixture())
        icon = app / "sce_sys" / "icon0.png"
        output = work / "output.exe"

        def relink():
            output.unlink(missing_ok=True)
            return subprocess.run([str(relinker), "--skip-sce-module", "--windows", str(source), str(output)],
                                  cwd=work, capture_output=True, text=True, timeout=20)

        result = relink()
        assert result.returncode == 0, (result.stdout, result.stderr)
        assert resources(output.read_bytes()) is None
        icon.parent.mkdir()
        for width, height in ((32, 32), (128, 64), (256, 256), (512, 512), (1024, 1024)):
            image = png(width, height)
            icon.write_bytes(image)
            result = relink()
            assert result.returncode == 0, (result.stdout, result.stderr)
            embedded = resources(output.read_bytes())
            assert embedded is not None, "missing Windows icon resources"
            assert embedded[3] == image, "PNG resource differs from icon0.png"
            group = struct.unpack("<HHHBBBBHHIH", embedded[14])
            assert group == (0, 1, 1, width if width < 256 else 0, height if height < 256 else 0,
                             0, 0, 1, 32, len(image), 1), group
            check_extraction(output)
            if os.name == "nt":
                assert subprocess.run([str(output)], timeout=20).returncode == 42

        valid = png(32, 32)
        bad_crc = bytearray(valid)
        bad_crc[29] ^= 1
        malformed = (b"not a PNG", valid[:32], valid[:8] + struct.pack(">I", 12) + valid[12:],
                     png(0, 32), png(32, 0), bytes(bad_crc), valid[:-1], valid[:-12],
                     valid + b"garbage", valid[:33] + chunk(b"IEND", b""),
                     valid[:8] + chunk(b"IHDR", struct.pack(">IIBBBBB", 32, 32, 8, 6, 1, 0, 0)) + valid[33:])
        for image in malformed:
            icon.write_bytes(image)
            result = relink()
            assert result.returncode != 0 and not output.exists(), (result.stdout, result.stderr)
            assert "Invalid Windows icon PNG" in result.stderr and str(icon) in result.stderr, result.stderr
        icon.unlink()
        result = relink()
        assert result.returncode == 0 and resources(output.read_bytes()) is None
    print("Windows icon integration tests passed (5 images, 11 malformed files, missing icon)")


if __name__ == "__main__":
    main()
