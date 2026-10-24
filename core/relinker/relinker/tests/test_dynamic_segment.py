from pathlib import Path
import struct
import subprocess
import sys
import tempfile

from test_guest_intel_trampolines import main_fixture
from test_guest_needed_modules import module_with_needed
from test_optional_plt import fixture as optional_plt_fixture


def fixture():
    image = optional_plt_fixture()
    struct.pack_into("<H", image, 0x38, 6)
    for index in range(2, 6):
        struct.pack_into("<IIQQQQQQ", image, 64 + index * 56,
                         0x6fffff01, 0, 0, 0, 0, 0, 0, 1)
    return image


def strict_fixture():
    image = optional_plt_fixture([(4, 0x6a0)])
    struct.pack_into("<I", image, 64 + 4, 5)
    struct.pack_into("<IIII", image, 0x6a0, 1, 1, 0, 0)
    return image


def main():
    relinker = Path(sys.argv[1]).resolve()
    failures = []
    with tempfile.TemporaryDirectory(prefix="anyps5-dynamic-segment-") as directory:
        work = Path(directory)

        def convert(name, image, error=None, modes=([], ["--windows"], ["--macos"], ["unused-filter=1"])):
            source = work / (name + ".elf")
            source.write_bytes(image)
            for mode in modes:
                output = work / (name + "." + ("-".join(flag.lstrip("-") for flag in mode) or "linux"))
                result = subprocess.run([str(relinker), "--skip-sce-module", *mode, str(source), str(output)],
                                        capture_output=True, text=True, timeout=20)
                valid = result.returncode == 0 and output.exists() if error is None else (
                    result.returncode == 2 and error in result.stderr and not output.exists())
                if not valid:
                    failures.append((name, mode, result.returncode, result.stdout, result.stderr))

        image = fixture()
        size = struct.unpack_from("<Q", image, 120 + 32)[0]
        convert("valid", image)

        padded = fixture()
        struct.pack_into("<QQ", padded, 120 + 32, size + 16, size + 16)
        struct.pack_into("<qQ", padded, 0x400 + size, 5, 0xffffffffffffffff)
        convert("ignore-after-null", padded)

        at_end = fixture()
        at_end[-size:] = at_end[0x400:0x400 + size]
        struct.pack_into("<Q", at_end, 120 + 8, len(at_end) - size)
        convert("ends-at-eof", at_end)

        for name, offset, declared_size in (
            ("offset-past-eof", len(image) + 1, size),
            ("offset-max", 0xffffffffffffffff, size),
            ("offset-wrap", 0xfffffffffffffff0, 32),
            ("size-wrap", 0x400, 0xfffffffffffffff0),
            ("truncated-after-null", 0x400, len(image) - 0x400 + 16),
        ):
            malformed = fixture()
            struct.pack_into("<Q", malformed, 120 + 8, offset)
            struct.pack_into("<Q", malformed, 120 + 32, declared_size)
            convert(name, malformed, "Dynamic segment out of bounds")

        trailing = fixture()
        struct.pack_into("<QQ", trailing, 120 + 32, size + 0x18, size + 0x18)
        trailing[0x400 + size:0x400 + size + 0x18] = bytes(range(1, 0x19))
        convert("trailing-bytes-after-null", trailing)

        cut = fixture()
        struct.pack_into("<QQ", cut, 120 + 32, size - 8, size - 8)
        convert("partial-terminator", cut, "Unterminated dynamic segment")

        missing_null = fixture()
        struct.pack_into("<QQ", missing_null, 120 + 32, size - 16, size - 16)
        convert("missing-null", missing_null, "Unterminated dynamic segment")

        empty = fixture()
        struct.pack_into("<QQ", empty, 120 + 32, 0, 0)
        convert("empty", empty, "Unterminated dynamic segment")

        strict_modes = (["unused-filter=2"], ["--windows", "unused-filter=2"], ["--macos", "unused-filter=2"])
        strict_size = struct.unpack_from("<Q", strict_fixture(), 120 + 32)[0]
        strict = strict_fixture()
        struct.pack_into("<QQ", strict, 120 + 32, strict_size + 0x18, strict_size + 0x18)
        strict[0x400 + strict_size:0x400 + strict_size + 0x18] = bytes(range(1, 0x19))
        convert("strict-trailing-bytes-after-null", strict, modes=strict_modes)

        strict = strict_fixture()
        struct.pack_into("<QQ", strict, 120 + 32, strict_size - 8, strict_size - 8)
        convert("strict-partial-terminator", strict, "Unterminated dynamic segment", strict_modes)

        for name, delta, error in (("module-trailing-bytes", 8, None),
                                   ("module-partial-terminator", -8, "Unterminated dynamic segment")):
            module = module_with_needed((b"libkernel.prx",), (0,))
            declared = struct.unpack_from("<Q", module, 176 + 32)[0] + delta
            struct.pack_into("<QQ", module, 176 + 32, declared, declared)
            for mode in ([], ["--windows"], ["--macos"]):
                case = work / (name + "-" + (mode[0].lstrip("-") if mode else "linux"))
                (case / "prx").mkdir(parents=True)
                (case / "prx" / "consumer.prx").write_bytes(module)
                source = case / "input.elf"
                source.write_bytes(main_fixture())
                output = case / "output"
                result = subprocess.run([str(relinker), *mode, str(source), str(output)],
                                        capture_output=True, text=True, timeout=30)
                valid = result.returncode == 0 and output.exists() if error is None else (
                    result.returncode == 2 and error in result.stderr and not output.exists())
                if not valid:
                    failures.append((name, mode, result.returncode, result.stdout, result.stderr))

    if failures:
        raise AssertionError(failures)
    print("Dynamic segment tests passed")


if __name__ == "__main__":
    main()
