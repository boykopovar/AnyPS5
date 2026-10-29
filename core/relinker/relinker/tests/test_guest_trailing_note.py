from pathlib import Path
import struct
import subprocess
import sys
import tempfile

from test_guest_intel_trampolines import main_fixture
from test_guest_module_directories import module_with_symbol


def module_with_note(offset, address, memory_size):
    image = module_with_symbol(True)
    struct.pack_into("<H", image, 56, 4)
    struct.pack_into("<IIQQQQQQ", image, 232, 4, 4, offset, address, address, 0x48, memory_size, 4)
    return image


def main():
    relinker = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="anyps5-trailing-note-") as directory:
        work = Path(directory)
        size = len(module_with_symbol(True))
        cases = (
            ("unmapped-past-eof", module_with_note(size, 0, 0), None),
            ("unmapped-in-file", module_with_note(size - 0x48, 0, 0), None),
            ("mapped-past-eof", module_with_note(size, 0x3000, 0x48), "ELF range exceeds file"),
        )
        for name, module, error in cases:
            for windows in (False, True):
                case = work / f"{name}-{windows}"
                (case / "sce_module").mkdir(parents=True)
                (case / "sce_module" / "noted.prx").write_bytes(module)
                source = case / "input.elf"
                source.write_bytes(main_fixture())
                output = case / ("output.exe" if windows else "output.elf")
                result = subprocess.run([str(relinker), *(["--windows"] if windows else []), str(source), str(output)],
                                        capture_output=True, text=True, timeout=30)
                artifact = case / "app0" / "sce_module" / "noted.prx.guest.prx"
                if error is None:
                    assert result.returncode == 0 and artifact.exists(), (name, windows, result.stdout, result.stderr)
                else:
                    assert result.returncode == 2 and error in result.stderr and not artifact.exists(), (
                        name, windows, result.returncode, result.stdout, result.stderr)
    print("Guest trailing note tests passed")


if __name__ == "__main__":
    main()
