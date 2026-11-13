from pathlib import Path
import os
import struct
import subprocess
import sys
import tempfile

from test_guest_intel_trampolines import main_fixture
from test_guest_none_relocations import FUNCTION, TARGET, change_tag, module
from test_guest_symbol_names import dynamic_tags, file_offset


DTPMOD64 = 16
DTPOFF64 = 17
TPOFF64 = 18
TEMPLATE = 0x2780


def fixture(sce=False, tls=True, info=3, value=0, kind=DTPMOD64, addend=0, section=0, symbol=3, tls_memory=16):
    image = module([(TARGET, (symbol << 32) | kind, addend)], sce=sce)
    struct.pack_into("<I", image, 0x2244, 4)
    if sce:
        change_tag(image, 4, 4 * 24, 0x6100003f)
    struct.pack_into("<IBBHQQ", image, 0x2280 + 3 * 24, 0, info, 0, section, value, 0)
    if tls:
        count, = struct.unpack_from("<H", image, 56)
        struct.pack_into("<H", image, 56, count + 1)
        struct.pack_into("<IIQQQQQQ", image, 64 + count * 56, 7, 4, TEMPLATE, TEMPLATE, TEMPLATE, min(8, tls_memory), tls_memory, 8)
        image[TEMPLATE:TEMPLATE + 8] = bytes(range(1, 9))
    return image


def with_flags(image, flags):
    size, = struct.unpack_from("<Q", image, 176 + 32)
    struct.pack_into("<qQqQ", image, 0x2000 + size - 16, 30, flags, 0, 0)
    struct.pack_into("<QQ", image, 176 + 32, size + 16, size + 16)
    return image


def relocations(data):
    tags = dynamic_tags(data)
    start = file_offset(data, tags[7])
    return [struct.unpack_from("<QQq", data, position) for position in range(start, start + tags[8], 24)]


def main():
    if sys.argv[1] == "--load":
        import ctypes
        library = ctypes.CDLL(sys.argv[2])
        function = getattr(library, "shared#guest")
        function.restype = ctypes.c_int
        assert function() == 42
        offset = ctypes.c_int64.from_address(ctypes.cast(function, ctypes.c_void_p).value + TARGET - FUNCTION).value
        thread = ctypes.c_uint64()
        libc = ctypes.CDLL(None, use_errno=True)
        assert libc.syscall(158, 0x1003, ctypes.byref(thread)) == 0, ctypes.get_errno()
        assert offset < 0, offset
        assert ctypes.string_at(thread.value + offset, 4) == bytes(range(5, 9)), (offset, ctypes.string_at(thread.value + offset, 4))
        return
    relinker = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="anyps5-guest-own-tls-") as directory:
        work = Path(directory)

        def convert(name, system, image):
            case = work / name / system
            guest = case / "sce_module" / "module.prx"
            guest.parent.mkdir(parents=True)
            guest.write_bytes(image)
            source = case / "input.elf"
            source.write_bytes(main_fixture())
            output = case / ("output.exe" if system == "windows" else "output.elf")
            flags = {"linux": [], "windows": ["--windows"], "macos": ["--macos"]}[system]
            result = subprocess.run([str(relinker), *flags, str(source), str(output)],
                                    capture_output=True, text=True, timeout=10)
            assert guest.read_bytes() == image
            return result, case / "app0" / "sce_module" / "module.prx.guest.prx", output

        for sce in (False, True):
            for kind, addend in ((DTPMOD64, 0), (DTPOFF64, 4)):
                image = fixture(sce, kind=kind, addend=addend)
                for system in ("linux", "windows", "macos"):
                    result, artifact, output = convert(f"{sce}-{kind}", system, image)
                    assert result.returncode == 0, (sce, kind, system, result.stdout, result.stderr)
                    if system == "linux":
                        assert relocations(artifact.read_bytes()) == [(TARGET, kind, addend)], (sce, kind)
            for name, image in (("no-tls", fixture(sce, tls=False)),
                                ("valued-section", fixture(sce, value=8)),
                                ("notype", fixture(sce, info=0)),
                                ("dtpmod-addend", fixture(sce, addend=4)),
                                ("tpoff", fixture(sce, kind=TPOFF64)),
                                ("absolute-section", fixture(sce, section=0xfff1))):
                result, artifact, output = convert(f"{sce}-{name}", "linux", image)
                assert result.returncode == 2 and "Invalid guest TLS relocation" in result.stderr, (sce, name, result.stderr)
                assert not artifact.exists() and not output.exists(), name
            for symbol, info, section in ((3, 3, 0), (0, 3, 0), (3, 6, 1)):
                for kind, addend in ((DTPMOD64, 0), (DTPOFF64, 4), (TPOFF64, 4)):
                    if info == 3 and symbol == 3 and kind == TPOFF64:
                        continue
                    if info == 6:
                        image = fixture(sce, info=info, section=section, kind=kind, addend=addend)
                        for system in ("linux", "windows", "macos"):
                            result, artifact, output = convert(f"{sce}-defined-{kind}", system, image)
                            assert result.returncode == 0, (sce, kind, system, result.stdout, result.stderr)
                    image = fixture(sce, info=info, section=section, kind=kind, addend=addend, symbol=symbol, tls_memory=0)
                    for system in ("linux", "windows", "macos"):
                        name = f"{sce}-{symbol}-{info}-{kind}-empty-tls"
                        result, artifact, output = convert(name, system, image)
                        assert result.returncode == 2 and "Invalid guest TLS relocation" in result.stderr, (name, system, result.stdout, result.stderr)
                        assert not artifact.exists() and not output.exists(), (name, system)
            for flags in (0x10, 0x18):
                image = with_flags(fixture(sce, info=6, section=1, kind=TPOFF64, addend=4), flags)
                for system in ("linux", "windows", "macos"):
                    result, artifact, output = convert(f"{sce}-static-tls-{flags}", system, image)
                    assert result.returncode == 0, (sce, flags, system, result.stdout, result.stderr)
                    if system == "linux":
                        assert [info & 0xffffffff for _, info, _ in relocations(artifact.read_bytes())] == [TPOFF64], (sce, flags)
                        if sys.platform == "linux" and os.uname().machine == "x86_64":
                            loaded = subprocess.run([sys.executable, __file__, "--load", str(artifact)], capture_output=True, text=True, timeout=10)
                            assert loaded.returncode == 0, (sce, flags, loaded.returncode, loaded.stderr)
            for flags in (0x1, 0x4, 0x14):
                image = with_flags(fixture(sce, info=6, section=1, kind=TPOFF64, addend=4), flags)
                result, artifact, output = convert(f"{sce}-unsupported-flags-{flags}", "linux", image)
                assert result.returncode == 2 and "Unsupported guest dynamic flags" in result.stderr, (sce, flags, result.stderr)
    print("Guest own TLS relocation tests passed")


if __name__ == "__main__":
    main()
