from pathlib import Path
import subprocess
import sys
import tempfile

from test_guest_intel_trampolines import main_fixture


def identification_cases(relinker, directory):
    image = main_fixture()
    cases = [("sysv", image, False)]
    freebsd = bytearray(image)
    freebsd[7] = 9
    cases.append(("freebsd", freebsd, False))
    for field, values in ((4, (0, 1, 255)), (5, (0, 2, 255)), (6, (0, 2, 255))):
        for value in values:
            data = bytearray(image)
            data[field] = value
            cases.append((f"ident-{field}-{value}", data, True))
    for mode, flags in enumerate(([], ["--windows"], ["--to-intel"], ["--windows", "--to-intel"])):
        for name, data, invalid in cases:
            source = Path(directory) / f"{mode}-{name}.elf"
            output = source.with_suffix(".out")
            registry = output.with_name(output.stem + ".registry.json")
            source.write_bytes(data)
            result = subprocess.run([str(relinker), "--skip-sce-module", "--registry", *flags, str(source), str(output)],
                                    capture_output=True, text=True, timeout=20)
            assert source.read_bytes() == data, (mode, name, "source changed")
            if invalid:
                assert result.returncode == 2 and "Expected little-endian ELF64 version 1" in result.stderr, (mode, name, result)
                assert not output.exists() and not registry.exists(), (mode, name, "output written for unsupported ELF identification")
            else:
                assert result.returncode == 0 and registry.is_file(), (mode, name, result)
                assert output.read_bytes().startswith(b"MZ" if "--windows" in flags else b"\x7fELF"), (mode, name)


def main():
    relinker = Path(sys.argv[1]).resolve()
    cases = [
        ("tiny", bytes(10), "File too small for ELF header"),
        ("truncated", b"\x7fELF\x02\x01\x01" + bytes(25), "File too small for ELF header"),
        ("header-only", b"\x7fELF\x02\x01\x01" + bytes(57), "No PT_DYNAMIC segment found"),
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
        identification_cases(relinker, directory)
    print("ELF header tests passed")


if __name__ == "__main__":
    main()
