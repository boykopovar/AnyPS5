import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

from test_optional_plt import fixture
from test_windows_import_modules import executable, provider


def pe_headers(data):
    pe = struct.unpack_from("<I", data, 0x3c)[0]
    optional = pe + 24
    return pe, optional


def file_offset(data, rva):
    pe, optional = pe_headers(data)
    table = optional + struct.unpack_from("<H", data, pe + 20)[0]
    for index in range(struct.unpack_from("<H", data, pe + 6)[0]):
        size, start, raw_size, raw = struct.unpack_from("<IIII", data, table + index * 40 + 8)
        if start <= rva and rva - start < min(size, raw_size):
            return raw + rva - start
    raise AssertionError(f"RVA {rva:#x} has no file data")


def image_end(path):
    data = path.read_bytes()
    _, optional = pe_headers(data)
    return struct.unpack_from("<Q", data, optional + 24)[0] + struct.unpack_from("<I", data, optional + 56)[0]


def absolute_jump(base):
    return bytes.fromhex("48 a1") + struct.pack("<Q", base + 0x300) + bytes.fromhex("ff e0")


def run(output, expected=42):
    if os.name == "nt":
        result = subprocess.run([str(output)], capture_output=True, timeout=5)
        assert result.returncode == expected, (output.name, result.returncode, result.stdout, result.stderr)


def check_occupied_base(output):
    if os.name != "nt":
        return
    import ctypes
    ctypes.windll.kernel32.SetErrorMode(0x8003)
    try:
        result = subprocess.run([str(output)], capture_output=True, timeout=5)
    except OSError as error:
        assert error.winerror == 487, error
    else:
        assert result.returncode & 0xffffffff == 0xc0000018, (result.returncode, result.stdout, result.stderr)


def main():
    relinker = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="anyps5-fixed-base-") as directory:
        work = Path(directory)

        def convert(name, image, base=None, options=(), modules=False):
            case = work / name
            case.mkdir(exist_ok=True)
            source = case / "input.elf"
            output = case / "output.exe"
            source.write_bytes(image)
            arguments = [str(relinker), "--windows"]
            if not modules:
                arguments.append("--skip-sce-module")
            if base is not None:
                arguments += ["--windows-fixed-base", hex(base)]
            result = subprocess.run([*arguments, *options, str(source), str(output)], capture_output=True, text=True, timeout=5)
            assert result.returncode == 0, (name, result.returncode, result.stdout, result.stderr)
            data = output.read_bytes()
            pe, optional = pe_headers(data)
            assert struct.unpack_from("<Q", data, optional + 24)[0] == (0x400000 if base is None else base) - 0x10000
            assert bool(struct.unpack_from("<H", data, pe + 22)[0] & 1) == (base is not None)
            characteristics = struct.unpack_from("<H", data, optional + 70)[0]
            assert characteristics & 0x100
            assert characteristics & 0x40 == 0, "converted main executable must not advertise DYNAMIC_BASE"
            relocations = struct.unpack_from("<II", data, optional + 112 + 5 * 8)
            assert bool(relocations[1]) == (base is None)
            return output

        default = convert("default", fixture())
        run(default)
        image = fixture()
        struct.pack_into("<H", image, 16, 0xfe10)
        code = bytes.fromhex("b8 00 03 00 00 48 83 b8 00 00 40 00 00 74 06 b8 2a 00 00 00 c3 31 c0 c3")
        image[0x200:0x200 + len(code)] = code
        image[0x240:0x246] = bytes.fromhex("b8 2a 00 00 00 c3")
        struct.pack_into("<q", image, 0x710, 0x240)
        run(convert("sce-dynexec-imports-offset-default", image))
        run(convert("sce-dynexec-imports-offset", image, 0x400000))
        for base in (0x400000, 0x800000, 0x400000000):
            image = fixture()
            code = absolute_jump(base)
            image[0x200:0x200 + len(code)] = code
            output = convert(f"absolute-{base:x}", image, base)
            data = output.read_bytes()
            assert struct.unpack_from("<Q", data, file_offset(data, 0x10300))[0] == base + 0x210
            for _ in range(3):
                run(output)

        image = fixture()
        code = bytes.fromhex("64 48 8b 04 25 f0 ff ff ff ff e0")
        image[0x200:0x200 + len(code)] = code
        struct.pack_into("<H", image, 0x38, 3)
        struct.pack_into("<IIQQQQQQ", image, 176, 7, 4, 0x800, 0x800, 0x800, 8, 16, 8)
        struct.pack_into("<Q", image, 0x400 + 5 * 16 + 8, 48)
        struct.pack_into("<QQq", image, 0x718, 0x800, 8, 0x210)
        tls = convert("tls", image, 0x400000)
        run(tls)
        data = tls.read_bytes()
        _, optional = pe_headers(data)
        tls_rva = struct.unpack_from("<I", data, optional + 112 + 9 * 8)[0]
        start, end, index, callbacks = struct.unpack_from("<QQQQ", data, file_offset(data, tls_rva))
        size = struct.unpack_from("<I", data, optional + 56)[0]
        assert all(0x3f0000 <= value < 0x3f0000 + size for value in (start, end, index, callbacks))
        assert struct.unpack_from("<Q", data, file_offset(data, start - 0x3f0000))[0] == 0x400210

        case = work / "guest-import"
        (case / "prx").mkdir(parents=True)
        (case / "prx" / "provider.prx").write_bytes(provider(42))
        image = executable("provider.prx")
        code = bytes.fromhex("48 a1") + struct.pack("<Q", 0x400a20) + bytes.fromhex("ff d0 c3")
        image[0x4000:0x4000 + len(code)] = code
        imported = convert("guest-import", image, 0x400000, modules=True)
        run(imported)
        guest = case / "app0" / "prx" / "provider.prx.guest.prx"
        guest_bytes = guest.read_bytes()
        _, guest_optional = pe_headers(guest_bytes)
        assert struct.unpack_from("<Q", guest_bytes, guest_optional + 24)[0] == 0x140000000

        for lazy in (False, True):
            output = convert(f"native-import-{lazy}", image, 0x400000, ("--lazy-binding",) if lazy else ())
            (output.parent / "libs").mkdir()
            (output.parent / "libs" / "provider.prx").write_bytes(guest_bytes)
            run(output)

        collision = convert("shared-user-data-collision", fixture(), 0x7ffe0000)
        check_occupied_base(collision)

        source = work / "bounds.elf"
        output = work / "bounds.exe"
        image = fixture()
        struct.pack_into("<Q", image, 64 + 40, 0x20000)
        source.write_bytes(image)
        result = subprocess.run([str(relinker), "--windows", "--skip-sce-module", "--windows-fixed-base", "0x7fffffff0000", str(source), str(output)], capture_output=True, text=True, timeout=5)
        assert result.returncode == 2 and "user address space" in result.stderr and not output.exists(), result

        size = image_end(convert("user-limit-size", fixture(), 0x400000)) - 0x3f0000
        assert size % 0x10000, hex(size)
        image = fixture()
        struct.pack_into("<Q", image, 64 + 40, struct.unpack_from("<Q", image, 64 + 40)[0] + 0x10000 - size % 0x10000)
        padded = image_end(convert("user-limit-padded-size", image, 0x400000)) - 0x3f0000
        assert padded % 0x10000 == 0, hex(padded)
        assert image_end(convert("user-limit-end", image, 0x7fffffff0000 - padded + 0x10000)) == 0x7fffffff0000

        base = (0x800000000000 - size + 0xffff) & ~0xffff
        assert 0x7fffffff0000 < base - 0x10000 + size <= 0x7ffffffff000, hex(base)
        source = work / "user-limit-top.elf"
        output = work / "user-limit-top.exe"
        source.write_bytes(fixture())
        result = subprocess.run([str(relinker), "--windows", "--skip-sce-module", "--windows-fixed-base", hex(base), str(source), str(output)], capture_output=True, text=True, timeout=5)
        assert result.returncode == 2 and "user address space" in result.stderr and not output.exists(), (hex(base), result)
    print("Windows fixed base: default, absolute GOT, 64-bit base, TLS, guest/native/lazy imports, collision, bounds and user address limit passed")


if __name__ == "__main__":
    main()
