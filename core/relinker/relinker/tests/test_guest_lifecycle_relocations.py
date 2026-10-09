from pathlib import Path
import platform
import struct
import subprocess
import sys
import tempfile

from test_guest_intel_trampolines import main_fixture


def fixture(relocations, entries):
    rela_offset = 0x3000
    targets = rela_offset + relocations * 24
    image = bytearray(targets + relocations * 8)
    image[:16] = b"\x7fELF\x02\x01\x01" + bytes(9)
    struct.pack_into("<HHIQQQIHHHHHH", image, 16, 3, 62, 1, 0x1000, 64, 0, 0, 64, 56, 3, 64, 0, 0)
    struct.pack_into("<IIQQQQQQ", image, 64, 1, 5, 0x1000, 0x1000, 0x1000, 0x100, 0x100, 0x1000)
    size = len(image) - 0x2000
    struct.pack_into("<IIQQQQQQ", image, 120, 1, 6, 0x2000, 0x2000, 0x2000, size, size, 0x1000)
    tags = [(5, 0x2200), (10, 8), (6, 0x2280), (11, 24), (4, 0x2240),
            (7, rela_offset), (8, relocations * 24), (9, 24),
            (25, targets), (27, entries * 8),
            (26, targets + (relocations - entries) * 8), (28, entries * 8), (0, 0)]
    struct.pack_into("<IIQQQQQQ", image, 176, 2, 6, 0x2000, 0x2000, 0x2000, len(tags)*16, len(tags)*16, 8)
    for index, tag in enumerate(tags):
        struct.pack_into("<qQ", image, 0x2000 + index*16, *tag)
    image[0x1000:0x1100] = b"\x90" * 0x100
    code = b"\xff\x05" + struct.pack("<i", 0x23C0 - 0x1006)
    code += b"\x8b\x05" + struct.pack("<i", 0x23C0 - 0x100C) + b"\xc3"
    image[0x1000:0x1000 + len(code)] = code
    image[0x2200:0x2208] = b"\0shared\0"
    struct.pack_into("<IIIII", image, 0x2240, 1, 2, 1, 0, 0)
    struct.pack_into("<IBBHQQ", image, 0x2298, 1, 0x12, 0, 1, 0x1000, len(code))
    for index in range(relocations):
        target_index = relocations - index - 1
        struct.pack_into("<QQq", image, rela_offset + index*24, targets + target_index*8, 8, 0x1000)
    return image

def check_loaded(artifact, count):
    if sys.platform != "linux" or platform.machine().lower() not in ("x86_64", "amd64"):
        return
    program = "import ctypes,sys; lib=ctypes.CDLL(sys.argv[1]); f=getattr(lib,'shared#guest'); f.restype=ctypes.c_int; assert f()==int(sys.argv[2])+1"
    result = subprocess.run([sys.executable, "-c", program, str(artifact), str(count)],
                            capture_output=True, text=True, timeout=5)
    assert result.returncode == 0, (result.returncode, result.stdout, result.stderr)


def main():
    relinker = Path(sys.argv[1]).resolve()
    cases = 0
    with tempfile.TemporaryDirectory(prefix="anyps5-guest-lifecycle-") as directory:
        root = Path(directory)

        def convert(name, image, windows, error=None, count=None):
            nonlocal cases
            cases += 1
            case = root / f"{name}-{windows}"
            (case / "sce_module").mkdir(parents=True)
            (case / "sce_module/module.prx").write_bytes(image)
            source = case / "input.elf"
            source.write_bytes(main_fixture())
            output = case / ("output.exe" if windows else "output.elf")
            artifact = case / "app0/sce_module/module.prx.guest.prx"
            result = subprocess.run([str(relinker), *(["--windows"] if windows else []),
                                     str(source), str(output)], capture_output=True, text=True, timeout=10)
            if error is not None:
                assert result.returncode == 2 and error in result.stderr, (name, result.returncode, result.stdout, result.stderr)
                assert not output.exists() and not artifact.exists(), name
                return
            assert result.returncode == 0 and output.exists() and artifact.exists(), (name, result.stdout, result.stderr)
            if not windows and count is not None:
                check_loaded(artifact, count)

        for windows in (False, True):
            for relocations, entries in ((12, 0), (12, 2), (4096, 64)):
                convert(f"relative-{relocations}-{entries}", fixture(relocations, entries), windows, count=entries)
            for finalization in (False, True):
                index = 1 if finalization else 11
                relocation = 0x3000 + index * 24
                original = fixture(12, 2)
                target, = struct.unpack_from("<Q", original, relocation)

                image = original.copy()
                struct.pack_into("<QQ", image, relocation + 8, (1 << 32) | 1, 0)
                convert(f"symbol-{finalization}", image, windows, count=2)

                image = original.copy()
                struct.pack_into("<QQ", image, relocation + 8, (1 << 32) | 6, 0)
                convert(f"glob-dat-{finalization}", image, windows, "Unsupported guest lifecycle relocation")

                image = original.copy()
                struct.pack_into("<QQ", image, relocation + 8, (1 << 32) | 1, 1)
                convert(f"addend-{finalization}", image, windows, "Invalid guest lifecycle function relocation")

                image = original.copy()
                image[0x229C] = 0x11
                struct.pack_into("<QQ", image, relocation + 8, (1 << 32) | 1, 0)
                convert(f"object-{finalization}", image, windows, "Invalid guest lifecycle function relocation")

                image = original.copy()
                struct.pack_into("<Q", image, relocation + 16, 0x23C0)
                convert(f"not-executable-{finalization}", image, windows, "Unmapped or inaccessible guest address")

                image = original.copy()
                struct.pack_into("<Q", image, relocation, 0x23E0)
                struct.pack_into("<Q", image, target, 0x1000)
                convert(f"unrelocated-{finalization}", image, windows, "Unrelocated guest lifecycle pointer")

                image = original.copy()
                struct.pack_into("<Q", image, relocation, 0x23E0)
                struct.pack_into("<QQq", image, 0x2300, target, (1 << 32) | 7, 0)
                for tag_index, tag in enumerate(((23, 0x2300), (2, 24), (20, 7), (0, 0)), 12):
                    struct.pack_into("<qQ", image, 0x2000 + tag_index * 16, *tag)
                struct.pack_into("<QQ", image, 208, 256, 256)
                convert(f"plt-{finalization}", image, windows, "Unsupported guest lifecycle relocation")

                for displacement in (-4, 0, 4):
                    image = original.copy()
                    next_target, = struct.unpack_from("<Q", image, 0x3000 + ((index + 2) % 12) * 24)
                    struct.pack_into("<Q", image, relocation, next_target + displacement)
                    convert(f"overlap-{finalization}-{displacement}", image, windows, "Overlapping guest relocations")
    print(f"Guest lifecycle relocation tests passed: {cases} conversions")


if __name__ == "__main__":
    main()
