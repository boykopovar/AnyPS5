from pathlib import Path
import struct
import subprocess
import sys
import tempfile

from guest_import_fixtures import dynamic
from test_guest_intel_trampolines import PLAIN_SITE, guest_fixture, main_fixture


STRINGS = b"\0tls_value\0a.prx\0"
SLOT = 0x2340
GLOB_DAT = 6
JUMP_SLOT = 7
DTPMOD64 = 16
DTPOFF64 = 17
REJECTED = "Non-TLS relocation against guest TLS export: tls_value"
MISMATCH = "Guest import/export type mismatch: tls_value"


def symbol_table(image, info, section, size, value=0):
    image[0x800:0x800 + len(STRINGS)] = STRINGS
    struct.pack_into("<IIIII", image, 0x840, 1, 2, 1, 0, 0)
    struct.pack_into("<IBBHQQ", image, 0x898, 1, info, 0, section, value, size)


def provider(tls=True):
    image = guest_fixture(PLAIN_SITE)
    if tls:
        struct.pack_into("<H", image, 56, 4)
        struct.pack_into("<IIQQQQQQ", image, 232, 7, 4, 0x980, 0x2380, 0x2380, 8, 0x40, 16)
        symbol_table(image, 0x16, 1, 8)
    else:
        symbol_table(image, 0x11, 2, 8, 0x2380)
    dynamic(image, 0x600, 176, [(5, 0x2200), (10, len(STRINGS)), (6, 0x2280), (11, 24),
                                (4, 0x2240), (7, 0x2300), (8, 0), (9, 24)])
    return image


def consumer(info, kinds, plt=False):
    image = guest_fixture(PLAIN_SITE)
    symbol_table(image, info, 0, 0)
    for index, kind in enumerate(kinds):
        struct.pack_into("<QQq", image, 0x900 + index * 24, SLOT + index * 8, (1 << 32) | kind, 0)
    size = len(kinds) * 24
    tags = [(5, 0x2200), (10, len(STRINGS)), (6, 0x2280), (11, 24), (4, 0x2240),
            (7, 0x2300), (8, 0 if plt else size), (9, 24), (1, 11)]
    if plt:
        tags += [(23, 0x2300), (2, size), (20, 7)]
    dynamic(image, 0x600, 176, tags)
    return image


def main():
    relinker = Path(sys.argv[1]).resolve()
    cases = (("notype-glob-dat", provider(), consumer(0x10, (GLOB_DAT,)), REJECTED),
             ("notype-64", provider(), consumer(0x10, (1,)), REJECTED),
             ("notype-jump-slot", provider(), consumer(0x10, (JUMP_SLOT,), plt=True), REJECTED),
             ("object-glob-dat", provider(), consumer(0x11, (GLOB_DAT,)), MISMATCH),
             ("notype-unreferenced", provider(), consumer(0x10, ()), None),
             ("notype-object-export", provider(False), consumer(0x10, (GLOB_DAT,)), None),
             ("tls-dtpmod-dtpoff", provider(), consumer(0x16, (DTPMOD64, DTPOFF64)), None))
    with tempfile.TemporaryDirectory(prefix="anyps5-guest-tls-export-references-") as directory:
        work = Path(directory)
        for name, exporter, image, message in cases:
            for system in ("linux", "windows", "macos"):
                case = work / name / system
                modules = case / "sce_module"
                modules.mkdir(parents=True)
                (modules / "a.prx").write_bytes(exporter)
                (modules / "b.prx").write_bytes(image)
                source = case / "input.elf"
                source.write_bytes(main_fixture())
                output = case / ("output.exe" if system == "windows" else "output.elf")
                flags = {"linux": [], "windows": ["--windows"], "macos": ["--macos"]}[system]
                result = subprocess.run([str(relinker), *flags, str(source), str(output)],
                                        capture_output=True, text=True, timeout=10)
                artifacts = case / "app0" / "sce_module"
                if message is None:
                    assert result.returncode == 0, (name, system, result.stdout, result.stderr)
                    assert output.exists() and (artifacts / "b.prx.guest.prx").exists(), (name, system)
                else:
                    assert result.returncode == 2 and message in result.stderr, (name, system, result.returncode, result.stderr)
                    assert not output.exists() and not artifacts.exists(), (name, system)
    print("Guest TLS export reference tests passed")


if __name__ == "__main__":
    main()
