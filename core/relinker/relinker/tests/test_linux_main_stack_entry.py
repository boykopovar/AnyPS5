"""Check that a Linux relink of an executable that loads libkernel.prx enters the guest entry through EnterGuestMainStack_nid_no_patch."""

from pathlib import Path
import os
import shutil
import struct
import subprocess
import sys
import tempfile

from test_guest_host_libc import importing
from test_guest_symbol_names import dynamic_tags, file_offset, imported_symbols, symbol_name

HELPER = "EnterGuestMainStack_nid_no_patch"
ENTRY = 0x4000
DEFAULT_BASE = 0x400000
EXIT_WITH_ARGC = bytes.fromhex("8b07" "8d7829" "b83c000000" "0f05")
PROLOGUE = bytes.fromhex("4889e7" "4883e4f0" "6a006a00" "4889e5")


def relink(relinker, case, library):
    case.mkdir()
    image = importing(library)
    image[ENTRY:ENTRY + len(EXIT_WITH_ARGC)] = EXIT_WITH_ARGC
    source = case / "input.elf"
    source.write_bytes(image)
    output = case / "output.elf"
    result = subprocess.run([str(relinker), "--skip-syscall-check", "--skip-sce-module", str(source), str(output)],
                            capture_output=True, text=True, timeout=30)
    assert result.returncode == 0, (library, result.stdout, result.stderr)
    return output.read_bytes()


def entry_stub(image):
    entry, = struct.unpack_from("<Q", image, 24)
    offset = file_offset(image, entry)
    return entry, image[offset:offset + 32]


def rip_target(entry, stub, end):
    displacement, = struct.unpack_from("<i", stub, end - 4)
    return entry + end + displacement


def helper_slots(image):
    tags = dynamic_tags(image)
    table = file_offset(image, tags[7])
    slots = []
    for position in range(table, table + tags[8], 24):
        target, info, addend = struct.unpack_from("<QQq", image, position)
        if info >> 32 and symbol_name(image, tags, info >> 32) == HELPER:
            symbol = file_offset(image, tags[6]) + (info >> 32) * 24
            slots.append((target, info & 0xffffffff, addend, image[symbol + 4], struct.unpack_from("<H", image, symbol + 6)[0]))
    return slots


def main():
    relinker = Path(sys.argv[1]).resolve()
    host = Path(sys.argv[2]).resolve()
    with tempfile.TemporaryDirectory(prefix="anyps5-main-stack-entry-") as directory:
        work = Path(directory)

        plain = relink(relinker, work / "without-kernel", "libc.prx")
        assert HELPER not in imported_symbols(plain)
        entry, stub = entry_stub(plain)
        assert stub[:14] == PROLOGUE and stub[14:18] == bytes.fromhex("4831f6e8"), stub.hex()
        assert rip_target(entry, stub, 22) == ENTRY + DEFAULT_BASE and stub[22:24] == bytes.fromhex("0f0b"), stub.hex()

        kernel = relink(relinker, work / "with-kernel", "libkernel.prx")
        slots = helper_slots(kernel)
        assert len(slots) == 1, slots
        slot, kind, addend, info, section = slots[0]
        assert (kind, addend, info, section) == (6, 0, 0x12, 0), slots
        entry, stub = entry_stub(kernel)
        assert stub[:14] == PROLOGUE and stub[14:17] == bytes.fromhex("488d35"), stub.hex()
        assert rip_target(entry, stub, 21) == ENTRY + DEFAULT_BASE and stub[21:23] == bytes.fromhex("ff15"), stub.hex()
        assert rip_target(entry, stub, 27) == slot and stub[27:29] == bytes.fromhex("0f0b"), stub.hex()

        if sys.platform == "linux" and os.uname().machine == "x86_64":
            case = work / "with-kernel"
            (case / "libs").mkdir()
            shutil.copyfile(host, case / "libs" / "libkernel.prx")
            output = case / "output.elf"
            output.chmod(0o755)
            for arguments, expected in (([], 42), (["extra"], 43)):
                run = subprocess.run([str(output), *arguments], capture_output=True, text=True, timeout=30)
                assert run.returncode == expected, (arguments, run.returncode, run.stdout, run.stderr)
    print("Linux main stack entry tests passed")


if __name__ == "__main__":
    main()
