from pathlib import Path
import os
import struct
import subprocess
import sys
import tempfile

from test_guest_intel_trampolines import PLAIN_SITE, elf_loads, guest_fixture, main_fixture


def module_with_symbol(exported):
    image = guest_fixture(PLAIN_SITE)
    name = b"\0shared#A#B\0"
    image[0x800:0x800 + len(name)] = name
    struct.pack_into("<Q", image, 0x618, len(name))
    struct.pack_into("<Q", image, 0x628, 0x2280)
    struct.pack_into("<IIIII", image, 0x840, 1, 2, 1, 0, 0)
    struct.pack_into("<IBBHQQ", image, 0x898, 1, 0x12, 0,
                     1 if exported else 0, 0x1000 if exported else 0, 1 if exported else 0)
    if not exported:
        struct.pack_into("<Q", image, 0x668, 24)
        struct.pack_into("<QQq", image, 0x900, 0x2320, (1 << 32) | 6, 0)
    return image


def needed_libraries(image):
    loads = elf_loads(image)

    def offset(address):
        for header in loads:
            if header[3] <= address < header[3] + header[5]:
                return header[2] + address - header[3]
        raise AssertionError(f"Unmapped address: {address:#x}")

    phoff, = struct.unpack_from("<Q", image, 32)
    phsize, phcount = struct.unpack_from("<HH", image, 54)
    dynamic = next(struct.unpack_from("<IIQQQQQQ", image, phoff + index * phsize)
                   for index in range(phcount)
                   if struct.unpack_from("<I", image, phoff + index * phsize)[0] == 2)
    tags = []
    for position in range(dynamic[2], dynamic[2] + dynamic[5], 16):
        tag, value = struct.unpack_from("<qQ", image, position)
        if tag == 0:
            break
        tags.append((tag, value))
    strings = offset(next(value for tag, value in tags if tag == 5))
    return [image[strings + value:image.index(0, strings + value)].decode()
            for tag, value in tags if tag == 1]


def main():
    relinker = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="anyps5-module-directories-") as directory:
        work = Path(directory)

        def convert(case, windows, options=()):
            source = case / "input.elf"
            source.write_bytes(main_fixture())
            output = case / ("output.exe" if windows else "output.elf")
            result = subprocess.run([str(relinker), *(["--windows"] if windows else []),
                                     *options, str(source), str(output)],
                                    capture_output=True, text=True, timeout=30)
            return result, output

        for windows in (False, True):
            for standard in (None, "sce_module", "sce_modules"):
                case = work / f"{windows}-{standard}"
                prx = case / "prx"
                prx.mkdir(parents=True)
                provider = (case / standard) if standard else prx
                provider.mkdir(exist_ok=True)
                (provider / "provider.prx").write_bytes(module_with_symbol(True))
                (prx / "consumer.prx").write_bytes(module_with_symbol(False))
                (prx / "ignored.txt").write_text("not ELF")
                (prx / "old.prx.guest.prx").write_bytes(guest_fixture(PLAIN_SITE))
                result, output = convert(case, windows)
                assert result.returncode == 0, (result.stdout, result.stderr)
                artifacts = list((case / "app0").rglob("*.guest.prx"))
                expected = {case / "app0" / (standard or "prx") / "provider.prx.guest.prx",
                            case / "app0" / "prx" / "consumer.prx.guest.prx"}
                assert set(artifacts) == expected, artifacts
                for artifact in artifacts:
                    assert artifact.read_bytes().startswith(b"MZ" if windows else b"\x7fELF")
                    assert f"    {artifact.relative_to(case / 'app0').as_posix()}\n" in result.stdout, result.stdout
                if windows and os.name == "nt":
                    run = subprocess.run([str(output)], capture_output=True, text=True, timeout=30)
                    assert run.returncode == 42, (run.returncode, run.stdout, run.stderr)
                if not windows:
                    needed = needed_libraries(output.read_bytes())
                    assert needed == [f"$ORIGIN/app0/{standard or 'prx'}/provider.prx.guest.prx",
                                      "$ORIGIN/app0/prx/consumer.prx.guest.prx"], needed
                    consumer = case / "app0" / "prx" / "consumer.prx.guest.prx"
                    needed = needed_libraries(consumer.read_bytes())
                    assert needed == [f"$ORIGIN/{'../' + standard + '/' if standard else ''}provider.prx.guest.prx"], needed

            case = work / f"{windows}-exclude"
            for name in ("sce_module", "prx"):
                (case / name).mkdir(parents=True)
                (case / name / "omit.prx").write_bytes(module_with_symbol(True))
            result, output = convert(case, windows, ["--exclude-sce-module", "omit.prx"])
            assert result.returncode == 0 and output.exists(), result.stderr
            assert not (case / "app0").exists(), case
            result, _ = convert(case, windows, ["--exclude-sce-module", "missing.prx"])
            assert result.returncode == 2 and "file not found" in result.stderr, result.stderr

            for magic in (b"\x54\x14\xf5\xee", b"\x4f\x15\x3d\x1d"):
                case = work / f"{windows}-self-{magic.hex()}"
                (case / "sce_module").mkdir(parents=True)
                wrapped = case / "sce_module" / "libc.prx"
                wrapped.write_bytes(magic + b"\x00\x01\x01\x12" + bytes(0x1000))
                result, output = convert(case, windows)
                assert result.returncode == 2, (result.stdout, result.stderr)
                assert "The bundled module is a SELF container, not an ELF" in result.stderr and str(wrapped) in result.stderr, result.stderr
                assert not output.exists() and not (case / "app0").exists(), output
                result, output = convert(case, windows, ["--exclude-sce-module", "libc.prx"])
                assert result.returncode == 0 and output.exists(), result.stderr

            case = work / f"{windows}-invalid"
            case.mkdir()
            (case / "prx").write_text("not a directory")
            result, output = convert(case, windows)
            assert result.returncode == 2 and "not a directory" in result.stderr, result.stderr
            assert not output.exists(), output

            case = work / f"{windows}-ambiguous"
            for name in ("sce_module", "sce_modules", "prx"):
                (case / name).mkdir(parents=True)
            result, output = convert(case, windows)
            assert result.returncode == 2 and "Both sce_module and sce_modules" in result.stderr, result.stderr
            assert not output.exists(), output

            case = work / f"{windows}-duplicate"
            for name in ("sce_module", "prx"):
                (case / name).mkdir(parents=True)
                (case / name / f"{name}.prx").write_bytes(module_with_symbol(True))
            result, output = convert(case, windows)
            assert result.returncode == 0 and output.exists(), result.stderr
            assert {path.name for path in (case / "app0").rglob("*.guest.prx")} == {"sce_module.prx.guest.prx", "prx.prx.guest.prx"}

            case = work / f"{windows}-ambiguous-import"
            for name in ("sce_module", "prx"):
                (case / name).mkdir(parents=True)
                (case / name / f"{name}.prx").write_bytes(module_with_symbol(True))
            (case / "prx" / "consumer.prx").write_bytes(module_with_symbol(False))
            result, output = convert(case, windows)
            assert result.returncode == 2 and "Ambiguous guest import" in result.stderr and "shared" in result.stderr, result.stderr
            assert not output.exists() and not (case / "app0").exists(), output
    print("Guest module directory integration tests passed")


if __name__ == "__main__":
    main()
