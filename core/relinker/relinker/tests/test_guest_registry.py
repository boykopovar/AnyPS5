import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

from test_guest_intel_trampolines import main_fixture
from test_guest_symbol_names import guest_module, importing_executable

RELA = 0x900
RELASZ = 0x668
PLT = 0x940
GLOB_DAT = 6
JUMP_SLOT = 7


def libc_module():
    image = guest_module([("AAAAAAAAAAA#A#A", True), ("BBBBBBBBBBB#B#B", False), ("CCCCCCCCCCC#C#C", False), ("EEEEEEEEEEE", True)])
    relocations = [(0x2380, 2, GLOB_DAT), (0x2388, 3, GLOB_DAT)]
    for index, (target, symbol, kind) in enumerate(relocations):
        struct.pack_into("<QQq", image, RELA + index * 24, target, (symbol << 32) | kind, 0)
    struct.pack_into("<Q", image, RELASZ, len(relocations) * 24)
    struct.pack_into("<QQq", image, PLT, 0x2390, (3 << 32) | JUMP_SLOT, 0)
    for index, tag in enumerate([(23, 0x2340), (2, 24), (20, 7), (0, 0)], 8):
        struct.pack_into("<qQ", image, 0x600 + index * 16, *tag)
    struct.pack_into("<QQ", image, 208, 12 * 16, 12 * 16)
    return image


def convert(relinker, case, options, sibling=True, registry=True):
    (case / "sce_module").mkdir(parents=True)
    source = case / "input.elf"
    source.write_bytes(main_fixture() if "--windows" in options else importing_executable(["AAAAAAAAAAA#A#A", "DDDDDDDDDDD#D#D"]))
    (case / "sce_module" / "libc.prx").write_bytes(libc_module())
    if sibling:
        (case / "sce_module" / "other.prx").write_bytes(guest_module([("BBBBBBBBBBB#B#B", True), ("EEEEEEEEEEE", True)]))
    output = case / "output.elf"
    result = subprocess.run([str(relinker), *(["--registry"] if registry else []), *options, str(source), str(output)], capture_output=True, text=True, timeout=30)
    assert result.returncode == 0, (options, result.stdout, result.stderr)
    return output


def read(path):
    return json.loads(path.read_text(encoding="utf-8"))


def entry(nid, offset):
    return {"nid": nid, "library": "", "targetOffset": offset}


def main():
    relinker = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="anyps5-guest-registry-") as directory:
        work = Path(directory)
        for name, options in (("linux", []), ("windows", ["--windows"])):
            output = convert(relinker, work / name, options)
            libc = read(output.parent / "output.libc.prx.guest.prx.registry.json")
            assert libc == [entry("CCCCCCCCCCC", "0x2388"), entry("CCCCCCCCCCC", "0x2390")], (name, libc)
            other = read(output.parent / "output.other.prx.guest.prx.registry.json")
            assert other == [], (name, other)
            main_nids = [item["nid"] for item in read(output.parent / "output.registry.json")]
            expected = [] if options else ["AAAAAAAAAAA#A#A", "DDDDDDDDDDD#D#D"]
            assert main_nids == expected, (name, main_nids)

            alone = convert(relinker, work / (name + "-alone"), options, sibling=False)
            libc = read(alone.parent / "output.libc.prx.guest.prx.registry.json")
            assert libc == [entry("BBBBBBBBBBB", "0x2380"), entry("CCCCCCCCCCC", "0x2388"), entry("CCCCCCCCCCC", "0x2390")], (name, libc)

            plain = convert(relinker, work / (name + "-plain"), options, registry=False)
            written = sorted(path.name for path in plain.parent.glob("*.registry.json"))
            assert written == [], (name, written)
    print("Guest module registry tests passed")


if __name__ == "__main__":
    main()
