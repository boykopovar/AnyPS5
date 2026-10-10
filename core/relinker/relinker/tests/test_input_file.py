"""Reject directories and other non-files before allocating an input buffer."""

import os
from pathlib import Path
import subprocess
import sys
import tempfile


def main():
    relinker = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="anyps5-input-file-") as directory:
        work = Path(directory)
        source_dir = work / "game"
        source_dir.mkdir()
        missing = work / "missing.elf"
        short = work / "short.elf"
        short.write_bytes(b"\x7fELF" + bytes(6))

        def convert(name, source, error, flags):
            output = work / (name + ".out")
            registry = work / (name + ".registry.json")
            result = subprocess.run([str(relinker), "--skip-sce-module", "--registry", *flags, str(source), str(output)],
                                    capture_output=True, text=True, timeout=20)
            assert result.returncode == 2 and error in result.stderr and "bad_alloc" not in result.stderr, (name, flags, result)
            assert not output.exists() and not registry.exists(), (name, flags, result)

        for flags in ([], ["--windows"], ["--to-intel"], ["--windows", "--to-intel"]):
            convert("directory" + "-".join(flags), source_dir, "Cannot open file: " + str(source_dir), flags)
        convert("missing", missing, "Cannot open file: " + str(missing), [])
        convert("short", short, "File too small for ELF header", [])
        if os.name == "posix":
            convert("null", Path("/dev/null"), "Cannot open file: /dev/null", [])
            fifo = work / "fifo.elf"
            os.mkfifo(fifo)
            convert("fifo", fifo, "Cannot open file: " + str(fifo), [])
    print("Input file tests passed")


if __name__ == "__main__":
    main()
