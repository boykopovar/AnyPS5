from pathlib import Path
import os
import struct
import subprocess
import sys
import tempfile

from test_guest_intel_trampolines import main_fixture
from test_guest_module_directories import module_with_symbol, needed_libraries


NEEDED = b"needed.prx"


def executable_with_needed():
    image = main_fixture()
    strings = b"\0" + NEEDED + b"\0"
    image[0x4800:0x4800 + len(strings)] = strings
    tags = []
    for position in range(0x4600, 0x4600 + 0x200, 16):
        tag, value = struct.unpack_from("<qQ", image, position)
        if tag == 0:
            break
        tags.append((tag, value))
    tags = [(5, 0x4800) if tag == 5 else (10, len(strings)) if tag == 10 else (tag, value) for tag, value in tags]
    tags += [(1, 1), (0, 0)]
    for index, tag in enumerate(tags):
        struct.pack_into("<qQ", image, 0x4600 + index * 16, *tag)
    struct.pack_into("<QQ", image, 120 + 32, len(tags) * 16, len(tags) * 16)
    return image


def main():
    relinker = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="anyps5-needed-modules-") as directory:
        work = Path(directory)

        def convert(case, windows):
            (case / "sce_module").mkdir(parents=True, exist_ok=True)
            source = case / "input.elf"
            source.write_bytes(executable_with_needed())
            output = case / ("output.exe" if windows else "output.elf")
            result = subprocess.run([str(relinker), *(["--windows"] if windows else []), str(source), str(output)],
                                    capture_output=True, text=True, timeout=30)
            return result, output

        for windows in (False, True):
            case = work / f"{windows}-found"
            modules = case / "Media" / "Modules"
            modules.mkdir(parents=True)
            (modules / "needed.prx").write_bytes(module_with_symbol(True))
            (case / "Media" / "needed.prx").write_text("not ELF")
            result, output = convert(case, windows)
            assert result.returncode == 0, (result.stdout, result.stderr)
            artifact = case / "app0" / "Media" / "Modules" / "needed.prx.guest.prx"
            assert artifact.read_bytes().startswith(b"MZ" if windows else b"\x7fELF"), artifact
            assert list((case / "app0").rglob("*.guest.prx")) == [artifact]
            assert "    Media/Modules/needed.prx.guest.prx\n" in result.stdout, result.stdout
            if windows and os.name == "nt":
                run = subprocess.run([str(output)], capture_output=True, text=True, timeout=30)
                assert run.returncode == 42, (run.returncode, run.stdout, run.stderr)
            if not windows:
                needed = needed_libraries(output.read_bytes())
                assert needed == ["$ORIGIN/app0/Media/Modules/needed.prx.guest.prx"], needed

            case = work / f"{windows}-ambiguous"
            for name in ("first", "second"):
                (case / name).mkdir(parents=True)
                (case / name / "needed.prx").write_bytes(module_with_symbol(True))
            result, output = convert(case, windows)
            assert result.returncode == 2 and "Ambiguous needed module" in result.stderr, result.stderr
            assert not output.exists(), output
    print("Guest needed module tests passed")


if __name__ == "__main__":
    main()
