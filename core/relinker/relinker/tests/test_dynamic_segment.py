from pathlib import Path
import struct
import subprocess
import sys
import tempfile

from test_optional_plt import fixture as optional_plt_fixture


def fixture():
    image = optional_plt_fixture()
    struct.pack_into("<H", image, 0x38, 5)
    for index in range(2, 5):
        struct.pack_into("<IIQQQQQQ", image, 64 + index * 56,
                         0x6fffff01, 0, 0, 0, 0, 0, 0, 1)
    return image


def main():
    relinker = Path(sys.argv[1]).resolve()
    failures = []
    with tempfile.TemporaryDirectory(prefix="anyps5-dynamic-segment-") as directory:
        work = Path(directory)

        def convert(name, image, error=None):
            source = work / (name + ".elf")
            source.write_bytes(image)
            for mode in ([], ["--windows"]):
                output = work / (name + (".exe" if mode else ".out"))
                result = subprocess.run([str(relinker), "--skip-sce-module", "--registry", *mode, str(source), str(output)],
                                        capture_output=True, text=True, timeout=20)
                if error is None:
                    valid = (result.returncode == 0 and output.exists()
                             and output.with_suffix(".registry.json").exists())
                else:
                    valid = (result.returncode == 2 and error in result.stderr and not output.exists()
                             and not output.with_suffix(".registry.json").exists())
                if not valid:
                    failures.append((name, mode, result.returncode, result.stdout, result.stderr))

        image = fixture()
        size = struct.unpack_from("<Q", image, 120 + 32)[0]
        convert("valid", image)

        missing = fixture()
        struct.pack_into("<I", missing, 120, 0x6fffff01)
        convert("missing", missing, "No PT_DYNAMIC segment found")

        for name, offset, declared_size in (
            ("same-table", 0x400, size),
            ("different-table", 0x800, 16),
            ("empty-table", 0x800, 0),
            ("out-of-bounds-table", 0xffffffffffffffff, size),
        ):
            for first in (False, True):
                duplicate = fixture()
                duplicate[0x800:0x810] = bytes(16)
                struct.pack_into("<IIQQQQQQ", duplicate, 176,
                                 2, 6, offset, 0x800, 0x800, declared_size, declared_size, 8)
                if first:
                    original = duplicate[120:176]
                    duplicate[120:176] = duplicate[176:232]
                    duplicate[176:232] = original
                convert("duplicate-" + name + ("-first" if first else "-last"),
                        duplicate, "Duplicate dynamic segment")

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

        partial = fixture()
        struct.pack_into("<Q", partial, 120 + 32, size + 1)
        convert("partial-entry", partial, "Invalid dynamic segment size")

        missing_null = fixture()
        struct.pack_into("<QQ", missing_null, 120 + 32, size - 16, size - 16)
        convert("missing-null", missing_null, "Unterminated dynamic segment")

        empty = fixture()
        struct.pack_into("<QQ", empty, 120 + 32, 0, 0)
        convert("empty", empty, "Unterminated dynamic segment")

    if failures:
        raise AssertionError(failures)
    print("Dynamic segment tests passed")


if __name__ == "__main__":
    main()
