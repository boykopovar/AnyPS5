from pathlib import Path
import errno
import os
import struct
import subprocess
import sys
import tempfile

from test_optional_plt import fixture


def main():
    relinker = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="anyps5-output-identity-") as directory:
        work = Path(directory)
        source = work / "input.elf"
        image = fixture()
        struct.pack_into("<H", image, 0x38, 5)
        for index in range(2, 5):
            struct.pack_into("<IIQQQQQQ", image, 64 + index * 56,
                             0x6FFFFF01, 0, 0, 0, 0, 0, 0, 1)
        original = bytes(image)
        source.write_bytes(original)
        (work / "nested").mkdir()
        hard_link = work / "hard-link.elf"
        os.link(source, hard_link)
        aliases = [
            (source, source),
            ("input.elf", source),
            (source, "input.elf"),
            (source, work / "nested" / ".." / "input.elf"),
            (source, hard_link),
        ]
        symbolic_link = work / "symbolic-link.elf"
        try:
            symbolic_link.symlink_to(source)
        except OSError as error:
            if error.errno not in (errno.ENOSYS, errno.EOPNOTSUPP) and getattr(error, "winerror", None) != 1314:
                raise
            print("Skipping symbolic-link case:", error)
        else:
            aliases.append((source, symbolic_link))
        if os.name == "nt":
            upper_source = work / "INPUT.ELF"
            if upper_source.exists() and upper_source.samefile(source):
                aliases.append((source, upper_source))

        def convert(input_path, output_path, windows, registry=False):
            command = [str(relinker), "--skip-sce-module"]
            if windows:
                command.append("--windows")
            if registry:
                command.append("--registry")
            return subprocess.run([*command, str(input_path), str(output_path)], cwd=work,
                                  capture_output=True, text=True, timeout=20)

        for windows in (False, True):
            for input_path, output_path in aliases:
                entries = set(work.iterdir())
                result = convert(input_path, output_path, windows, registry=True)
                assert result.returncode == 2, (input_path, output_path, result.stdout, result.stderr)
                assert "Output file must not refer to the input file" in result.stderr, result.stderr
                assert source.read_bytes() == original, "input was modified"
                assert Path(work / output_path).read_bytes() == original, "alias was modified"
                assert set(work.iterdir()) == entries, "artifacts were written before rejecting the output"

            for existing in (False, True):
                output = work / (f"distinct-{windows}-{existing}" + (".exe" if windows else ".elf"))
                if existing:
                    output.write_bytes(b"previous output")
                result = convert(source, output, windows)
                assert result.returncode == 0, (result.stdout, result.stderr)
                converted = output.read_bytes()
                assert converted.startswith(b"MZ" if windows else b"\x7fELF"), "invalid output magic"
                assert converted != original, "input was not converted"
                assert source.read_bytes() == original, "distinct output modified the input"
    print("Output file identity integration tests passed")


if __name__ == "__main__":
    main()
