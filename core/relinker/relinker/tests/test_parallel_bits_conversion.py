from pathlib import Path
import subprocess
import sys
import tempfile

from test_guest_intel_trampolines import guest_fixture, main_fixture
from test_to_rosetta import executable


def main():
    relinker = Path(sys.argv[1]).resolve()
    sites = (("PDEP", bytes.fromhex("c4e2f3f5c6")), ("PEXT", bytes.fromhex("c442b2f5c2")),
             ("PDEP", bytes.fromhex("c4e2f3f5442440")), ("PEXT", bytes.fromhex("c4e272f50540000000")))
    count = 0
    with tempfile.TemporaryDirectory(prefix="anyps5-parallel-bits-") as directory:
        work = Path(directory)
        for guest in (False, True):
            for macos in (False, True):
                for conversion in ((), ("--to-intel",), ("--to-rosetta",)):
                    for index, (name, site) in enumerate(sites):
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
                        assert (site not in data) == lowered, (case.name, f"{name} instruction presence")
                        if not guest:
                            assert (name in result.stdout) == lowered, (case.name, result.stdout)
                        if guest:
                            assert module.read_bytes() == original, case.name
                        count += 1
    print(f"{count} executable/guest PDEP and PEXT conversions passed across Linux and macOS output formats")


if __name__ == "__main__":
    main()
