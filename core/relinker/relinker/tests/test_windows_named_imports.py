import os
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent / "macos"))

from guest_import_fixtures import executable
from nid import nid
from test_guest_tls import guest_with_tls
from test_windows_import_modules import provider


def native_library(relinker, case, exported, value):
    modules = case / "prx"
    modules.mkdir(parents=True)
    image = provider(value)
    name = b"\0" + exported.encode() + b"\0"
    image[0x800:0x800 + len(name)] = name
    struct.pack_into("<Q", image, 0x618, len(name))
    (modules / "native.prx").write_bytes(image)
    (case / "input.elf").write_bytes(executable("native.prx", exported + "#A#B"))
    result = subprocess.run([str(relinker), "--windows", str(case / "input.elf"), str(case / "output.exe")],
                            capture_output=True, text=True, timeout=30)
    assert result.returncode == 0, (result.stdout, result.stderr)
    return (case / "app0" / "prx" / "native.prx.guest.prx").read_bytes()


def convert(relinker, case, library, needed, symbol):
    (case / "prx").mkdir(parents=True)
    (case / "libs").mkdir()
    (case / "prx" / "tls.prx").write_bytes(guest_with_tls(True))
    (case / "libs" / "available.prx").write_bytes(library)
    (case / "input.elf").write_bytes(executable(needed, symbol))
    output = case / "output.exe"
    result = subprocess.run([str(relinker), "--windows", str(case / "input.elf"), str(output)],
                            capture_output=True, text=True, timeout=30)
    assert result.returncode == 0, (result.stdout, result.stderr)
    return output


def run(output):
    return subprocess.run([str(output)], capture_output=True, text=True, timeout=30)


def main():
    relinker = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="anyps5-windows-named-imports-") as directory:
        work = Path(directory)
        by_nid = native_library(relinker, work / "nid-provider", nid("shared"), 22)
        by_name = native_library(relinker, work / "name-provider", "shared", 33)

        cases = {
            "nid": convert(relinker, work / "nid", by_nid, "available.prx", "shared"),
            "sprx": convert(relinker, work / "sprx", by_nid, "available.sprx", "shared"),
            "name": convert(relinker, work / "name", by_name, "available.prx", "shared"),
            "qualified": convert(relinker, work / "qualified", by_nid, "available.prx", "shared#A#B"),
            "missing": convert(relinker, work / "missing", by_nid, "available.prx", "absent"),
        }
        if os.name != "nt":
            return
        result = run(cases["nid"])
        assert result.returncode == 22, (result.returncode, result.stdout, result.stderr)
        result = run(cases["sprx"])
        assert result.returncode == 22, (result.returncode, result.stdout, result.stderr)
        result = run(cases["name"])
        assert result.returncode == 33, (result.returncode, result.stdout, result.stderr)
        result = run(cases["qualified"])
        assert result.returncode != 0 and "unresolved ELF import shared from available.prx" in result.stderr, (result.returncode, result.stderr)
        result = run(cases["missing"])
        assert result.returncode != 0 and "unresolved ELF import absent\n" in result.stderr, (result.returncode, result.stderr)
    print("Windows named import tests passed")


if __name__ == "__main__":
    main()
