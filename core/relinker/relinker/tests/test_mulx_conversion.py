from pathlib import Path
import subprocess
import sys
import tempfile

from test_guest_intel_trampolines import guest_fixture, main_fixture
from test_to_rosetta import executable


def main():
    relinker = Path(sys.argv[1]).resolve()
    sites = (bytes.fromhex("c4e273f6c6"), bytes.fromhex("c442b3f6c2"),
             bytes.fromhex("c4e2f3f6442440"), bytes.fromhex("c4e2f3f60540000000"))
    with tempfile.TemporaryDirectory(prefix="anyps5-mulx-") as directory:
        work = Path(directory)
        for guest in (False, True):
            for macos in (False, True):
                for conversion in ((), ("--to-intel",), ("--to-rosetta",)):
                    for index, site in enumerate(sites):
                        case = work / f"{guest}-{macos}-{conversion}-{index}"
                        case.mkdir()
                        source = case / "input.elf"
                        output = case / "output"
                        options = ["--macos"] if macos else []
                        if guest:
                            modules = case / "sce_module"
                            modules.mkdir()
                            module = modules / "sample.prx"
                            original = guest_fixture(site)
                            module.write_bytes(original)
                            source.write_bytes(main_fixture())
                        else:
                            options.append("--skip-sce-module")
                            source.write_bytes(executable(site + b"\xc3"))
                        result = subprocess.run([str(relinker), *options, *conversion, str(source), str(output)],
                                                capture_output=True, text=True, timeout=10)
                        assert result.returncode == 0, (case.name, result.stdout, result.stderr)
                        artifact = case / "app0/sce_module/sample.prx.guest.prx" if guest else output
                        data = artifact.read_bytes()
                        assert data[:4] == (bytes.fromhex("cffaedfe") if macos else b"\x7fELF"), case.name
                        lowered = conversion == ("--to-rosetta",)
                        assert (site not in data) == lowered, (case.name, "MULX instruction presence")
                        if not guest:
                            assert ("MULX" in result.stdout) == lowered, (case.name, result.stdout)
                        if guest:
                            assert module.read_bytes() == original, case.name
    print("48 executable/guest MULX conversions passed across Linux and macOS output formats")


if __name__ == "__main__":
    main()
