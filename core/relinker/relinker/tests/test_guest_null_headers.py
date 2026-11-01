from pathlib import Path
import ctypes
import platform
import struct
import subprocess
import sys
import tempfile

from test_guest_intel_trampolines import main_fixture
from test_guest_module_dir import host_module


def main():
    relinker = Path(sys.argv[1]).resolve()
    maximum = (1 << 64) - 1
    cases = (
        ("zero", 0, (0, 0, 0, 0, 0, 0, 0), None),
        ("past-eof", 0, (4, 0x100000, 0x3000, 0x3000, 0x4000, 0x4000, 4), None),
        ("empty-past-eof", 0, (0, maximum, 0, 0, 0, 0, 1), None),
        ("undefined-fields", 0, (0xffffffff, maximum, maximum, maximum, maximum, 0, 3), None),
        ("load-past-eof", 1, (4, 0x100000, 0x3000, 0x3000, 0x4000, 0x4000, 4), "Segment offset out of bounds"),
        ("note-past-eof", 4, (4, 0x100000, 0x3000, 0x3000, 0x4000, 0x4000, 4), "ELF range exceeds file"),
    )
    targets = (("linux", (), b"\x7fELF"), ("windows", ("--windows",), b"MZ"),
               ("macos", ("--macos",), b"\xcf\xfa\xed\xfe"))
    with tempfile.TemporaryDirectory(prefix="anyps5-null-headers-") as directory:
        for name, kind, fields, error in cases:
            for target, options, magic in targets:
                case = Path(directory) / f"{name}-{target}"
                (case / "sce_module").mkdir(parents=True)
                image = host_module()
                struct.pack_into("<H", image, 56, 5)
                struct.pack_into("<IIQQQQQQ", image, 288, kind, *fields)
                (case / "sce_module" / "null.prx").write_bytes(image)
                source = case / "input.elf"
                source.write_bytes(main_fixture())
                output = case / ("output.exe" if target == "windows" else "output.elf")
                result = subprocess.run([str(relinker), *options, str(source), str(output)],
                                        capture_output=True, text=True, timeout=30)
                artifact = case / "app0" / "sce_module" / "null.prx.guest.prx"
                if error is not None:
                    assert result.returncode == 2 and error in result.stderr and not artifact.exists(), (
                        name, target, result.returncode, result.stdout, result.stderr)
                    continue
                assert result.returncode == 0 and output.exists() and artifact.exists(), (
                    name, target, result.returncode, result.stdout, result.stderr)
                assert artifact.read_bytes().startswith(magic), (name, target)
                if target == "linux" and sys.platform == "linux" and platform.machine() == "x86_64":
                    ctypes.CDLL(str(artifact))
    print("Guest PT_NULL header tests passed")


if __name__ == "__main__":
    main()
