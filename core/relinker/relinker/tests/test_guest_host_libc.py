from pathlib import Path
import struct
import subprocess
import sys
import tempfile

from test_guest_intel_trampolines import PLAIN_SITE, guest_fixture, main_fixture
from test_guest_module_directories import needed_libraries

DYNAMIC = 0x4800
STRINGS = 0x4A00


def importing(*libraries):
    image = main_fixture()
    strings = b"\0" + b"".join(name.encode() + b"\0" for name in libraries)
    image[STRINGS:STRINGS + len(strings)] = strings
    tags = []
    offset = 1
    for name in libraries:
        tags.append((1, offset))
        offset += len(name) + 1
    tags += [(5, STRINGS), (10, len(strings)), (6, 0x4620), (11, 24), (7, 0x4700), (8, 0), (9, 24), (0, 0)]
    for index, tag in enumerate(tags):
        struct.pack_into("<qQ", image, DYNAMIC + index * 16, *tag)
    struct.pack_into("<QQQQQ", image, 120 + 8, DYNAMIC, DYNAMIC, DYNAMIC, len(tags) * 16, len(tags) * 16)
    return image


def convert(relinker, case, libraries, modules, options=()):
    (case / "sce_module").mkdir(parents=True)
    source = case / "input.elf"
    source.write_bytes(importing(*libraries))
    for name in modules:
        (case / "sce_module" / name).write_bytes(guest_fixture(PLAIN_SITE))
    output = case / "output.elf"
    result = subprocess.run([str(relinker), *options, str(source), str(output)],
                            capture_output=True, text=True, timeout=30)
    assert result.returncode == 0, (result.stdout, result.stderr)
    return needed_libraries(output.read_bytes())


def main():
    relinker = Path(sys.argv[1]).resolve()
    imports = ("libkernel.prx", "libc.prx", "libSceLibcInternal.prx")
    with tempfile.TemporaryDirectory(prefix="anyps5-guest-host-libc-") as directory:
        work = Path(directory)

        needed = convert(relinker, work / "bundled", imports, ["libc.prx", "other.prx"])
        assert needed == ["$ORIGIN/app0/sce_module/libc.prx.guest.prx",
                          "$ORIGIN/app0/sce_module/other.prx.guest.prx",
                          "libkernel.prx", "libSceLibcInternal.prx", "libc.prx"], needed

        needed = convert(relinker, work / "without-internal", ("libkernel.prx", "libc.prx"), ["libc.prx"])
        assert needed == ["$ORIGIN/app0/sce_module/libc.prx.guest.prx", "libkernel.prx"], needed

        needed = convert(relinker, work / "not-bundled", imports, ["other.prx"])
        assert needed == ["$ORIGIN/app0/sce_module/other.prx.guest.prx", *imports], needed

        needed = convert(relinker, work / "excluded", imports, ["libc.prx", "other.prx"],
                         ["--exclude-sce-module", "libc.prx"])
        assert needed == ["$ORIGIN/app0/sce_module/other.prx.guest.prx", *imports], needed
    print("Guest host libc integration tests passed")


if __name__ == "__main__":
    main()
