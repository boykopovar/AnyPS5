from pathlib import Path
import os
import struct
import subprocess
import sys
import tempfile

from test_guest_intel_trampolines import PLAIN_SITE, guest_fixture, main_fixture
from test_guest_module_directories import module_with_symbol, needed_libraries


NEEDED = b"needed.prx"


def executable_with_needed(needed=NEEDED, repeats=1):
    image = main_fixture()
    strings = b"\0" + needed + b"\0"
    image[0x4800:0x4800 + len(strings)] = strings
    tags = []
    for position in range(0x4600, 0x4600 + 0x200, 16):
        tag, value = struct.unpack_from("<qQ", image, position)
        if tag == 0:
            break
        tags.append((tag, value))
    tags = [(5, 0x4800) if tag == 5 else (10, len(strings)) if tag == 10 else (tag, value) for tag, value in tags]
    tags += [(1, 1)] * repeats + [(0, 0)]
    for index, tag in enumerate(tags):
        struct.pack_into("<qQ", image, 0x4600 + index * 16, *tag)
    struct.pack_into("<QQ", image, 120 + 32, len(tags) * 16, len(tags) * 16)
    return image


def module_with_needed(names, entries):
    image = guest_fixture(PLAIN_SITE)
    strings = bytearray(b"\0")
    offsets = []
    for name in names:
        offsets.append(len(strings))
        strings += name + b"\0"
    assert len(strings) <= 0x100
    image[0x900:0x900 + len(strings)] = strings
    tags = [(5, 0x2300), (10, len(strings)), (6, 0x2220), (11, 24),
            (4, 0x2240), (7, 0x2300), (8, 0), (9, 24)]
    tags += [(1, offsets[index]) for index in entries] + [(0, 0)]
    for index, tag in enumerate(tags):
        struct.pack_into("<qQ", image, 0x600 + index * 16, *tag)
    struct.pack_into("<QQ", image, 176 + 32, len(tags) * 16, len(tags) * 16)
    return image


def check_guest_needed(relinker, work):
    first, second = b"libkernel.prx", b"libSceVideoOut.prx"
    cases = (("single", (first,), (0,), [first.decode()]),
             ("adjacent", (first,), (0, 0), [first.decode()]),
             ("separated", (first, second), (0, 1, 0), [first.decode(), second.decode()]),
             ("distinct-offsets", (first, second, first), (0, 1, 2), [first.decode(), second.decode()]))
    for windows in (False, True):
        for label, names, entries, expected in cases:
            case = work / f"guest-{windows}-{label}"
            (case / "prx").mkdir(parents=True)
            source = case / "input.elf"
            source.write_bytes(main_fixture())
            (case / "prx" / "consumer.prx").write_bytes(module_with_needed(names, entries))
            output = case / ("output.exe" if windows else "output.elf")
            result = subprocess.run([str(relinker), *(["--windows"] if windows else []), str(source), str(output)],
                                    capture_output=True, text=True, timeout=30)
            assert result.returncode == 0, (label, windows, result.stdout, result.stderr)
            artifact = case / "app0" / "prx" / "consumer.prx.guest.prx"
            assert artifact.read_bytes().startswith(b"MZ" if windows else b"\x7fELF"), artifact
            if not windows:
                needed = needed_libraries(artifact.read_bytes())
                assert needed == expected, (label, needed)
                needed = needed_libraries(output.read_bytes())
                assert needed == ["$ORIGIN/app0/prx/consumer.prx.guest.prx", *expected], (label, needed)
        for label, invalid in (("empty", b""), ("dollar", b"bad$name.prx")):
            case = work / f"guest-{windows}-{label}"
            (case / "prx").mkdir(parents=True)
            source = case / "input.elf"
            source.write_bytes(main_fixture())
            module = case / "prx" / "consumer.prx"
            module.write_bytes(module_with_needed((first, invalid), (0, 0, 1)))
            output = case / ("output.exe" if windows else "output.elf")
            result = subprocess.run([str(relinker), *(["--windows"] if windows else []), str(source), str(output)],
                                    capture_output=True, text=True, timeout=30)
            assert result.returncode == 2 and f"{module}: Invalid dependency: {invalid.decode()}\n" in result.stderr, result.stderr
            assert not output.exists() and not (case / "app0").exists(), output


def main():
    relinker = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="anyps5-needed-modules-") as directory:
        work = Path(directory)
        check_guest_needed(relinker, work)

        def convert(case, windows, needed=NEEDED, repeats=1):
            if not (case / "sce_modules").exists():
                (case / "sce_module").mkdir(parents=True, exist_ok=True)
            source = case / "input.elf"
            source.write_bytes(executable_with_needed(needed, repeats))
            output = case / ("output.exe" if windows else "output.elf")
            result = subprocess.run([str(relinker), *(["--windows"] if windows else []), str(source), str(output)],
                                    capture_output=True, text=True, timeout=30)
            return result, output

        for windows in (False, True):
            case = work / f"{windows}-found"
            modules = case / "sce_module"
            modules.mkdir(parents=True)
            (modules / "needed.prx").write_bytes(module_with_symbol(True))
            (case / "Media").mkdir()
            (case / "Media" / "needed.prx").write_text("not ELF")
            result, output = convert(case, windows)
            assert result.returncode == 0, (result.stdout, result.stderr)
            artifact = case / "app0" / "sce_module" / "needed.prx.guest.prx"
            assert artifact.read_bytes().startswith(b"MZ" if windows else b"\x7fELF"), artifact
            assert list((case / "app0").rglob("*.guest.prx")) == [artifact]
            assert "    sce_module/needed.prx.guest.prx\n" in result.stdout, result.stdout
            if windows and os.name == "nt":
                run = subprocess.run([str(output)], capture_output=True, text=True, timeout=30)
                assert run.returncode == 42, (run.returncode, run.stdout, run.stderr)
            if not windows:
                needed = needed_libraries(output.read_bytes())
                assert needed == ["$ORIGIN/app0/sce_module/needed.prx.guest.prx"], needed

            case = work / f"{windows}-repeated-system"
            result, output = convert(case, windows, b"libSceVideoOut.prx", repeats=2)
            assert result.returncode == 0, (result.stdout, result.stderr)
            assert output.exists(), output

            for directory in ("sce_module", "sce_modules", "prx"):
                case = work / f"{windows}-debug-name-{directory.replace('/', '-')}"
                (case / directory).mkdir(parents=True)
                (case / directory / "needed.prx").write_bytes(module_with_symbol(True))
                result, output = convert(case, windows, b"needed.debug_prx")
                assert result.returncode == 0, (result.stdout, result.stderr)
                artifact = case / "app0" / directory / "needed.prx.guest.prx"
                assert list((case / "app0").rglob("*.guest.prx")) == [artifact], list((case / "app0").rglob("*"))
                if windows and os.name == "nt":
                    run = subprocess.run([str(output)], capture_output=True, text=True, timeout=30)
                    assert run.returncode == 42, (run.returncode, run.stdout, run.stderr)
                if not windows:
                    needed = needed_libraries(output.read_bytes())
                    assert needed == [f"$ORIGIN/app0/{directory}/needed.prx.guest.prx"], needed

            if windows:
                case = work / "debug-name-case"
                (case / "prx").mkdir(parents=True)
                (case / "prx" / "foo-bar.prx").write_bytes(module_with_symbol(True))
                result, output = convert(case, windows, b"Foo-Bar.debug_prx")
                assert result.returncode == 0, (result.stdout, result.stderr)
                artifact = case / "app0" / "prx" / "foo-bar.prx.guest.prx"
                assert list((case / "app0").rglob("*.guest.prx")) == [artifact], list((case / "app0").rglob("*"))
                if os.name == "nt":
                    run = subprocess.run([str(output)], capture_output=True, text=True, timeout=30)
                    assert run.returncode == 42, (run.returncode, run.stdout, run.stderr)

            case = work / f"{windows}-debug-name-ambiguous"
            (case / "first").mkdir(parents=True)
            (case / "first" / "needed.prx").write_bytes(module_with_symbol(True))
            (case / "first" / "needed.sprx").write_bytes(module_with_symbol(True))
            result, output = convert(case, windows, b"needed.debug_prx")
            assert result.returncode == 0, result.stderr
            assert output.exists(), output
            assert not list((case / "app0").rglob("*.guest.prx"))
            if not windows:
                assert needed_libraries(output.read_bytes()) == ["needed.debug_prx" if "debug-name" in case.name else "needed.prx"]

            case = work / f"{windows}-ambiguous"
            for name in ("first", "second"):
                (case / name).mkdir(parents=True)
                (case / name / "needed.prx").write_bytes(module_with_symbol(True))
            result, output = convert(case, windows)
            assert result.returncode == 0, result.stderr
            assert output.exists(), output
            assert not list((case / "app0").rglob("*.guest.prx"))
            if not windows:
                assert needed_libraries(output.read_bytes()) == ["needed.debug_prx" if "debug-name" in case.name else "needed.prx"]
        case = work / "macos-repeated-system"
        (case / "sce_module").mkdir(parents=True)
        source = case / "input.elf"
        source.write_bytes(executable_with_needed(b"libSceVideoOut.prx", 2))
        result = subprocess.run([str(relinker), "--macos", str(source), str(case / "output")],
                                capture_output=True, text=True, timeout=30)
        assert result.returncode == 0, (result.stdout, result.stderr)
    print("Guest needed module tests passed")


if __name__ == "__main__":
    main()
