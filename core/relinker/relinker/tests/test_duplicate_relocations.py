import struct
import subprocess
import sys
import tempfile
from pathlib import Path

from test_guest_none_relocations import module
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
    print("Duplicate relocation tests passed")


if __name__ == "__main__":
    main()
