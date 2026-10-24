from pathlib import Path
import struct
import subprocess
import sys
import tempfile

from test_guest_intel_trampolines import main_fixture


MODES = ([], ["--windows"], ["--to-intel"], ["--windows", "--to-intel"],
         ["--macos"], ["--macos", "--to-rosetta"])


def identification_cases(relinker, directory):
    image = main_fixture()
    cases = [("sysv", image, None)]
    freebsd = bytearray(image)
    freebsd[7] = 9
    cases.append(("freebsd", freebsd, None))
    for field, values in ((4, (0, 1, 255)), (5, (0, 2, 255)), (6, (0, 2, 255))):
        for value in values:
            data = bytearray(image)
            data[field] = value
            cases.append((f"ident-{field}-{value}", data, "Expected little-endian ELF64 version 1"))
    for value in (0, 3, 183):
        data = bytearray(image)
        data[18:20] = value.to_bytes(2, "little")
        cases.append((f"machine-{value}", data, "Expected an x86-64 ELF"))
    for mode, flags in enumerate(MODES):
        for name, data, error in cases:
            source = Path(directory) / f"{mode}-{name}.elf"
            output = source.with_suffix(".out")
            registry = output.with_name(output.stem + ".registry.json")
            source.write_bytes(data)
            result = subprocess.run([str(relinker), "--skip-sce-module", "--registry", *flags, str(source), str(output)],
                                    capture_output=True, text=True, timeout=20)
            assert source.read_bytes() == data, (mode, name, "source changed")
            if error:
                assert result.returncode == 2 and error in result.stderr, (mode, name, result)
                assert not output.exists() and not registry.exists(), (mode, name, "output written for unsupported ELF identification")
            else:
                assert result.returncode == 0 and registry.is_file(), (mode, name, result)
                magic = b"\x7fELF"
                if "--windows" in flags:
                    magic = b"MZ"
                elif "--macos" in flags:
                    magic = b"\xcf\xfa\xed\xfe"
                assert output.read_bytes().startswith(magic), (mode, name)


def structural_cases(relinker, directory):
    fields = (
        (20, "I", (0, 2, 0xFFFFFFFF), "Unsupported ELF object version"),
        (52, "H", (0, 63, 65, 65535), "Invalid ELF header size"),
    )
    for mode, flags in enumerate(MODES):
        for filter_level in (0, 1, 2):
            for offset, encoding, values, error in fields:
                for value in values:
                    case = Path(directory) / f"header-{mode}-{filter_level}-{offset}-{value}"
                    case.mkdir()
                    source = case / "input.elf"
                    output = case / "output.out"
                    registry = case / "output.registry.json"
                    data = main_fixture()
                    struct.pack_into("<" + encoding, data, offset, value)
                    source.write_bytes(data)
                    result = subprocess.run([str(relinker), "--skip-sce-module", "--registry",
                                             f"unused-filter={filter_level}", *flags, str(source), str(output)],
                                            capture_output=True, text=True, timeout=20)
                    assert result.returncode == 2 and error in result.stderr, (case.name, result)
                    assert source.read_bytes() == data, (case.name, "source changed")
                    assert not output.exists() and not registry.exists(), (case.name, "output written")


def object_type_cases(relinker, directory):
    cases = ((0, "ET_NONE inputs do not specify an ELF object type"),
             (1, "ET_REL relocatable inputs are not supported"),
             (4, "ET_CORE core dumps are not executable images"),
             (2, None), (3, None), (0xFE10, None), (0xFE18, None))
    for mode, flags in enumerate(MODES):
        for filter_level in (0, 1, 2):
            for object_type, error in cases:
                if object_type == 2 and ("--windows" in flags or "--macos" in flags):
                    error = "This target requires a position-independent ELF image"
                case = Path(directory) / f"type-{mode}-{filter_level}-{object_type}"
                case.mkdir()
                source = case / "input.elf"
                output = case / "output.out"
                registry = case / "output.registry.json"
                data = main_fixture()
                if filter_level == 2:
                    struct.pack_into("<qQqQ", data, 0x4670, 4, 0x46A0, 0, 0)
                    struct.pack_into("<QQ", data, 120 + 32, 9 * 16, 9 * 16)
                    struct.pack_into("<IIII", data, 0x46A0, 1, 1, 0, 0)
                struct.pack_into("<H", data, 16, object_type)
                source.write_bytes(data)
                result = subprocess.run([str(relinker), "--skip-sce-module", "--registry",
                                         f"unused-filter={filter_level}", *flags, str(source), str(output)],
                                        capture_output=True, text=True, timeout=20)
                assert source.read_bytes() == data, (case.name, "source changed")
                if error:
                    assert result.returncode == 2 and error in result.stderr, (case.name, result)
                    assert not output.exists() and not registry.exists(), (case.name, "output written")
                else:
                    assert result.returncode == 0 and registry.is_file(), (case.name, result)
                    magic = b"\x7fELF"
                    if "--windows" in flags:
                        magic = b"MZ"
                    elif "--macos" in flags:
                        magic = b"\xcf\xfa\xed\xfe"
                    assert output.read_bytes().startswith(magic), (case.name, "wrong output format")


def main():
    relinker = Path(sys.argv[1]).resolve()
    headerOnly = bytearray(64)
    headerOnly[0:7] = b"\x7fELF\x02\x01\x01"
    headerOnly[16:18] = (3).to_bytes(2, "little")
    headerOnly[18:20] = (62).to_bytes(2, "little")
    headerOnly[20:24] = (1).to_bytes(4, "little")
    headerOnly[52:54] = (64).to_bytes(2, "little")
    cases = [
        ("tiny", bytes(10), "File too small for ELF header"),
        ("truncated", b"\x7fELF\x02\x01\x01" + bytes(25), "File too small for ELF header"),
        ("header-only", bytes(headerOnly), "No PT_DYNAMIC segment found"),
        ("header-only-sections", main_fixture()[:64], "FileByteOffset out of bounds"),
        ("bad-magic", bytes(64), "Invalid ELF magic number"),
    ]
    with tempfile.TemporaryDirectory(prefix="anyps5-elf-header-") as directory:
        for name, data, error in cases:
            source = Path(directory) / (name + ".elf")
            output = source.with_suffix(".out")
            source.write_bytes(data)
            result = subprocess.run([str(relinker), "--skip-sce-module", str(source), str(output)],
                                    capture_output=True, text=True, timeout=20)
            assert result.returncode == 2 and error in result.stderr and not output.exists(), (name, result)

        object_type_cases(relinker, directory)
        identification_cases(relinker, directory)
        structural_cases(relinker, directory)
    print("ELF header tests passed")


if __name__ == "__main__":
    main()
