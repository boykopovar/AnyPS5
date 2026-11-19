import struct
import subprocess
import sys
import tempfile
from pathlib import Path

from test_guest_none_relocations import module
from test_guest_symbol_names import DYNAMIC, RELOCATIONS, importing_executable
from test_overlapping_relocations import TARGETS, check_output, fixture


def with_second_entry(entry):
    image, local, plt = fixture("disjoint", False, False)
    struct.pack_into("<QQq", image, 0x700 + 24, *entry)
    for offset in range(0x400, 0x500, 16):
        tag, value = struct.unpack_from("<qQ", image, offset)
        if tag == 8:
            struct.pack_into("<qQ", image, offset, tag, value + 24)
            return image, local, plt
    raise AssertionError("no DT_RELASZ tag")


def strict_fixture(second_addend=None):
    image = importing_executable(["live"])
    dynamic_size, = struct.unpack_from("<Q", image, 152)
    struct.pack_into("<qQ", image, DYNAMIC + dynamic_size - 16, 0x6100003F, 48)
    struct.pack_into("<qQ", image, DYNAMIC + dynamic_size, 0, 0)
    struct.pack_into("<QQ", image, 152, dynamic_size + 16, dynamic_size + 16)
    image[0x4020:0x4026] = b"\xb8\x11\x00\x00\x00\xc3"
    struct.pack_into("<QQq", image, RELOCATIONS + 24, 0x4E08, 8, 0x4020)
    if second_addend is not None:
        struct.pack_into("<QQq", image, RELOCATIONS + 48, 0x4E08, 8, second_addend)
    for offset in range(DYNAMIC, DYNAMIC + dynamic_size, 16):
        tag, value = struct.unpack_from("<qQ", image, offset)
        if tag == 8:
            struct.pack_into("<qQ", image, offset, tag, 48 if second_addend is None else 72)
            return image
    raise AssertionError("no DT_RELASZ tag")


def strict_convert(relinker, work, name, image):
    source = work / f"strict-{name}.elf"
    source.write_bytes(image)
    output = work / f"strict-{name}.out"
    result = subprocess.run([str(relinker), "--skip-sce-module", "unused-filter=2", str(source), str(output)],
                            capture_output=True, text=True, timeout=10)
    return result, output


def strict_reports(result):
    return (next(line for line in result.stdout.splitlines() if "Strict filtering total:" in line),
            next(line for line in result.stdout.splitlines() if line.startswith("Function graph:")))


def main():
    relinker = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="anyps5-relocation-duplicate-") as directory:
        work = Path(directory)

        def convert(name, target, options, image):
            case = work / f"{name}-{target}"
            modules = case / "sce_module"
            modules.mkdir(parents=True)
            (modules / "module.prx").write_bytes(module())
            source = case / "input.elf"
            source.write_bytes(image)
            output = case / ("output.exe" if target == "windows" else "output.out")
            result = subprocess.run([str(relinker), "--registry", *options, str(source), str(output)],
                                    capture_output=True, text=True, timeout=10)
            return result, output

        _, local, _ = fixture("disjoint", False, False)
        duplicate, _, plt = with_second_entry(local[0])
        for target, options in TARGETS:
            result, output = convert("duplicate", target, options, duplicate)
            assert result.returncode == 0, (target, result.stdout, result.stderr)
            if target != "linux":
                check_output(output.read_bytes(), target, False, local, plt)
        offset, info, addend = local[0]
        conflicting, _, _ = with_second_entry((offset, info, addend + 8))
        result, output = convert("conflicting", "windows", ["--windows"], conflicting)
        assert result.returncode == 2 and "Overlapping ELF relocation targets" in result.stderr and not output.exists(), (
            result.stdout, result.stderr)
        result, output = strict_convert(relinker, work, "single", strict_fixture())
        assert result.returncode == 0 and output.exists(), (result.stdout, result.stderr)
        expected = strict_reports(result)
        result, output = strict_convert(relinker, work, "duplicate", strict_fixture(0x4020))
        assert result.returncode == 0 and output.exists(), (result.stdout, result.stderr)
        assert strict_reports(result) == expected, (result.stdout, expected)
        result, output = strict_convert(relinker, work, "conflicting", strict_fixture(0x4030))
        assert result.returncode == 2 and "Strict filter: overlapping pointer relocations" in result.stderr and not output.exists(), (
            result.stdout, result.stderr)
    print("Duplicate relocation tests passed")


if __name__ == "__main__":
    main()
