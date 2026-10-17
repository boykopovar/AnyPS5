from pathlib import Path
import os
import platform
import struct
import subprocess
import sys
import tempfile

from test_guest_intel_trampolines import main_fixture
from test_guest_none_relocations import FUNCTION, TARGET, UNCHANGED, change_tag, check_output, module


def fixture(sce=False, index=1, lifecycle=False, info=3, value=0):
    image = module([(0, 0xffffffff00000000, -1), (TARGET, (index << 32) | 8, FUNCTION),
                    (TARGET + 8, 8, FUNCTION)], sce=sce, lifecycle=lifecycle)
    if index == 1:
        image[0x22B0:0x22C8] = image[0x2298:0x22B0]
    count = max(3, index + 1)
    struct.pack_into("<I", image, 0x2244, count)
    if sce:
        change_tag(image, 4, count * 24, 0x6100003f)
    struct.pack_into("<IBBHQQ", image, 0x2280 + index * 24, 0, info, 0, 0, value, 0)
    return image


def main():
    if sys.argv[1] == "--load":
        import ctypes
        library = ctypes.CDLL(sys.argv[2])
        function = getattr(library, "shared" if os.name == "nt" else "shared#guest")
        function.restype = ctypes.c_int
        assert function() == 42
        address = ctypes.cast(function, ctypes.c_void_p).value
        pointer = ctypes.c_uint64.from_address(address + TARGET - FUNCTION).value
        assert pointer == address, (hex(pointer), hex(address))
        assert ctypes.CFUNCTYPE(ctypes.c_int)(pointer)() == 42
        return
    relinker = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="anyps5-guest-relative-section-") as directory:
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
            for index, lifecycle in ((1, False), (3, False), (1, True)):
                image = fixture(sce, index, lifecycle)
                for system in ("linux", "windows", "macos"):
                    result, artifact, output = convert(f"{sce}-{index}-{lifecycle}", system, image)
                    assert result.returncode == 0, (sce, index, lifecycle, system, result.stdout, result.stderr)
                    check_output(artifact.read_bytes(), system, [(TARGET, 8, FUNCTION), (TARGET + 8, 8, FUNCTION)], [], UNCHANGED)
                    native = (system == "linux" and sys.platform == "linux") or (system == "windows" and os.name == "nt")
                    if native and platform.machine().lower() in ("x86_64", "amd64") and not lifecycle:
                        executed = subprocess.run([sys.executable, __file__, "--load", str(artifact)],
                                                  capture_output=True, text=True, timeout=10)
                        assert executed.returncode == 0, (executed.returncode, executed.stdout, executed.stderr)
            for name, info, value, error in (("valued-section", 3, 16, "RELATIVE relocation has a symbol"),
                                             ("notype", 0, 0, "RELATIVE relocation has a symbol"),
                                             ("global-section", 0x13, 0, "Unsupported symbol attributes")):
                result, artifact, output = convert(f"{sce}-{name}", "linux", fixture(sce, info=info, value=value))
                assert result.returncode == 2 and error in result.stderr, (name, result.stderr)
                assert not artifact.exists() and not output.exists(), name
            image = fixture(sce)
            struct.pack_into("<Q", image, 0x2320, (99 << 32) | 8)
            result, artifact, output = convert(f"{sce}-out-of-bounds", "linux", image)
            assert result.returncode == 2 and "Unsupported relocation or symbol index" in result.stderr, result.stderr
            assert not artifact.exists() and not output.exists()
    print("Guest relative section symbol tests passed")


if __name__ == "__main__":
    main()
