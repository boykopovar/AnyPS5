"""R_X86_64_COPY relocations in DT_RELA and in the PLT table are rejected by validation, before any
target code runs, with the same error for every output target; GLOB_DAT and JUMP_SLOT are accepted. The
Linux output gives every import symbol st_size 0, so a dynamic loader would copy MIN(st_size of the
definition, 0) = 0 bytes and the variable would stay zero."""

from pathlib import Path
import subprocess
import sys
import tempfile

from test_jmprel_table_bounds import fixture

R_X86_64_COPY = 5
TARGETS = {(): "linux", ("--lazy-binding",): "lazy", ("--windows",): "windows", ("--macos",): "macos"}
ERROR = "Unsupported relocation type 0x5"

CASES = [
    ("copy-rela", fixture(linux=True, data_relocations=True, relocation_type=R_X86_64_COPY), True),
    ("copy-jmprel", fixture(linux=True, relocation_type=R_X86_64_COPY), True),
    ("glob-dat-rela", fixture(linux=True, data_relocations=True), False),
    ("jump-slot-jmprel", fixture(linux=True), False),
]


def relink(relinker, work, name, image, flags):
    source = work / (name + ".elf")
    output = work / (name + ".out")
    source.write_bytes(image)
    result = subprocess.run([str(relinker), "--skip-sce-module", *flags, str(source), str(output)],
                            capture_output=True, text=True, timeout=20)
    return result, output


def fail_lines(result):
    return [line for line in result.stderr.splitlines() if line.startswith("FAIL: ")]


def main():
    relinker = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="anyps5-copy-") as directory:
        work = Path(directory)
        for case, image, rejected in CASES:
            failures = {}
            for flags, target in TARGETS.items():
                name = f"{case}-{target}"
                result, output = relink(relinker, work, name, image, flags)
                if rejected:
                    assert result.returncode == 2 and ERROR in result.stderr and not output.exists(), (
                        name, result.returncode, result.stdout, result.stderr)
                    failures[target] = fail_lines(result)
                else:
                    assert result.returncode == 0 and output.exists(), (
                        name, result.returncode, result.stdout, result.stderr)
                    output.unlink()
            if rejected:
                assert failures["linux"] and all(lines == failures["linux"] for lines in failures.values()), (
                    case, failures)
    print("COPY relocation test passed")


if __name__ == "__main__":
    main()
