"""Check that the Linux executable is placed at 0x400000 by default or at the --linux-fixed-base address, with every address it holds rebased, that --lazy-binding keeps it position independent and warns, and that the largest image the relinker accepts ends below the guest arena."""

from pathlib import Path
import platform
import signal
import struct
import subprocess
import sys
import tempfile

from signal_run import run_without_core_dump
from test_linux_load_alignment import PT_SCE_VERSION

PT_NULL = 0
PT_LOAD = 1
PT_DYNAMIC = 2
PT_GNU_STACK = 0x6474E551
BASE = 0x400000
HIGH_BASE = 0x400000000
SHIFT = HIGH_BASE - BASE
ENTRY = 0x10
SLOT = 0x800
ADDRESS_TAGS = {3, 4, 5, 6, 7, 23}
GUEST_ARENA_START = 0x200000000
LAZY_WARNING = "with --lazy-binding the Linux executable stays position independent"


def check_code(base):
    """lea rax,[rip-7]; mov rcx,[rip+slot]; cmp rax,rcx; jne fail; movabs rdx,base+entry; cmp rax,rdx; jne fail; int3; fail: ud2"""
    slot_displacement = SLOT - (ENTRY + 14)
    return (b"\x48\x8d\x05" + struct.pack("<i", -7)
            + b"\x48\x8b\x0d" + struct.pack("<i", slot_displacement)
            + b"\x48\x39\xc8" + b"\x75\x10"
            + b"\x48\xba" + struct.pack("<Q", base + ENTRY)
            + b"\x48\x39\xd0" + b"\x75\x01"
            + b"\xcc" + b"\x0f\x0b")


def fixture(relocation_type=8, addend=ENTRY, alignment=0x4000, memory_size=0x1000, slots=6):
    image = bytearray(0x8000)
    image[:16] = b"\x7fELF\x02\x01\x01" + bytes(9)
    struct.pack_into("<HHIQQQIHHHHHH", image, 16, 3, 62, 1, ENTRY, 64, 0, 0, 64, 56, slots, 64, 0, 0)
    tags = [(5, 0x7A0), (10, 1), (6, 0x780), (11, 24), (7, 0x700), (8, 24), (9, 24), (0, 0)]
    struct.pack_into("<IIQQQQQQ", image, 64, PT_LOAD, 7, 0x4000, 0, 0, 0x1000, memory_size, alignment)
    struct.pack_into("<IIQQQQQQ", image, 120, PT_DYNAMIC, 6, 0x4600, 0x600, 0x600, len(tags) * 16, len(tags) * 16, 8)
    for index in range(2, slots):
        struct.pack_into("<IIQQQQQQ", image, 64 + index * 56, PT_SCE_VERSION, 0, 0, 0, 0, 0, 0, 1)
    for index, tag in enumerate(tags):
        struct.pack_into("<qQ", image, 0x4600 + index * 16, *tag)
    struct.pack_into("<QQq", image, 0x4700, SLOT, relocation_type, addend)
    code = check_code(BASE)
    image[0x4000 + ENTRY:0x4000 + ENTRY + len(code)] = code
    return image


def headers(elf):
    phoff, = struct.unpack_from("<Q", elf, 0x20)
    phentsize, phnum = struct.unpack_from("<HH", elf, 0x36)
    return [struct.unpack_from("<IIQQQQQQ", elf, phoff + index * phentsize) for index in range(phnum)]


def mapped(elf, address):
    for header in headers(elf):
        if header[0] == PT_LOAD and header[3] <= address < header[3] + header[5]:
            return header[2] + address - header[3]
    raise AssertionError(hex(address))


def dynamic_tags(elf):
    dynamic = next(header for header in headers(elf) if header[0] == PT_DYNAMIC)
    return [struct.unpack_from("<qQ", elf, dynamic[2] + offset) for offset in range(0, dynamic[5], 16)]


def relocations(elf):
    tags = dict(dynamic_tags(elf))
    offset = mapped(elf, tags[7])
    return [struct.unpack_from("<QQq", elf, offset + index) for index in range(0, tags[8], 24)]


def relink(relinker, directory, name, image, *options):
    source = directory / (name + ".elf")
    output = directory / (name + ".out")
    source.write_bytes(image)
    result = subprocess.run([str(relinker), "--skip-sce-module", *options, str(source), str(output)],
                            capture_output=True, text=True, timeout=20)
    return result, output


def main():
    relinker = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="anyps5-fixed-base-") as directory:
        work = Path(directory)
        result, fixed_path = relink(relinker, work, "default", fixture())
        assert result.returncode == 0, (result.stdout, result.stderr)
        assert LAZY_WARNING not in result.stdout + result.stderr, (result.stdout, result.stderr)
        result, explicit_path = relink(relinker, work, "explicit", fixture(), "--linux-fixed-base", hex(BASE))
        assert result.returncode == 0, (result.stdout, result.stderr)
        result, high_path = relink(relinker, work, "high", fixture(), "--linux-fixed-base", hex(HIGH_BASE))
        assert result.returncode == 0, (result.stdout, result.stderr)
        result, lazy_path = relink(relinker, work, "lazy", fixture(), "--lazy-binding")
        assert result.returncode == 0, (result.stdout, result.stderr)
        assert LAZY_WARNING in result.stdout + result.stderr, (result.stdout, result.stderr)
        fixed = fixed_path.read_bytes()
        high = high_path.read_bytes()
        lazy = lazy_path.read_bytes()

        assert fixed == explicit_path.read_bytes(), "a relink without the option is not placed at 0x400000"
        assert struct.unpack_from("<H", fixed, 16)[0] == 2, "a default relink is not ET_EXEC"
        assert struct.unpack_from("<H", high, 16)[0] == 2, "a fixed-base relink is not ET_EXEC"
        assert struct.unpack_from("<H", lazy, 16)[0] == 3, "a lazy-binding relink is no longer ET_DYN"
        assert relocations(lazy) == [(SLOT, 8, ENTRY)], relocations(lazy)
        assert struct.unpack_from("<Q", high, 24)[0] == struct.unpack_from("<Q", fixed, 24)[0] + SHIFT, "entry not rebased"

        fixed_headers = headers(fixed)
        high_headers = headers(high)
        assert len(fixed_headers) == len(high_headers)
        for before, after in zip(fixed_headers, high_headers):
            shift = 0 if before[0] in (PT_NULL, PT_GNU_STACK) else SHIFT
            assert after == (*before[:3], before[3] + shift, before[4] + shift, *before[5:]), (before, after)
        assert min(header[3] for header in fixed_headers if header[0] == PT_LOAD) == BASE, "image does not start at the base"
        assert min(header[3] for header in high_headers if header[0] == PT_LOAD) == HIGH_BASE, "image does not start at the base"

        fixed_dynamic = dynamic_tags(fixed)
        high_dynamic = dynamic_tags(high)
        assert len(fixed_dynamic) == len(high_dynamic), (len(fixed_dynamic), len(high_dynamic))
        for (tag, value), (high_tag, high_value) in zip(fixed_dynamic, high_dynamic):
            assert tag == high_tag and high_value == value + (SHIFT if tag in ADDRESS_TAGS else 0), (tag, value, high_value)

        assert relocations(fixed) == [(SLOT + BASE, 8, ENTRY + BASE)], relocations(fixed)
        assert relocations(high) == [(SLOT + HIGH_BASE, 8, ENTRY + HIGH_BASE)], relocations(high)

        relocated = fixture(slots=5)
        result, relocated_fixed_path = relink(relinker, work, "relocated-default", relocated)
        assert result.returncode == 0, (result.stdout, result.stderr)
        result, relocated_high_path = relink(relinker, work, "relocated-high", relocated, "--linux-fixed-base", hex(HIGH_BASE))
        assert result.returncode == 0, (result.stdout, result.stderr)
        relocated_fixed = relocated_fixed_path.read_bytes()
        relocated_high = relocated_high_path.read_bytes()
        assert struct.unpack_from("<H", relocated_fixed, 0x38)[0] > struct.unpack_from("<H", relocated, 0x38)[0], "the output program header table did not grow past the input's slots"
        assert struct.unpack_from("<Q", relocated_fixed, 0x20)[0] != 0x40, "the input table had slots for the output layout"
        relocated_fixed_headers = headers(relocated_fixed)
        relocated_high_headers = headers(relocated_high)
        assert len(relocated_fixed_headers) == len(relocated_high_headers)
        for before, after in zip(relocated_fixed_headers, relocated_high_headers):
            shift = 0 if before[0] in (PT_NULL, PT_GNU_STACK) else SHIFT
            assert after == (*before[:3], before[3] + shift, before[4] + shift, *before[5:]), (before, after)
        assert min(header[3] for header in relocated_fixed_headers if header[0] == PT_LOAD) == BASE, "the relocated image does not start at the base"
        relocated_entry = struct.unpack_from("<Q", relocated_fixed, 24)[0]
        assert struct.unpack_from("<Q", relocated_high, 24)[0] == relocated_entry + SHIFT, "entry not rebased"
        entry_offset = mapped(relocated_fixed, relocated_entry)
        assert entry_offset < len(relocated_fixed), ("the entry's mapping lies outside the output file", entry_offset)
        covering = next(header for header in relocated_fixed_headers if header[0] == PT_LOAD and header[3] <= relocated_entry < header[3] + header[5])
        assert covering[1] & 1, "the segment covering the entry is not executable"

        large_path = None
        for far, accepted in ((0x7F000000, True), (0x80000000, False), (GUEST_ARENA_START - BASE - 0x1000, False)):
            spanning = fixture()
            struct.pack_into("<IIQQQQQQ", spanning, 64 + 2 * 56, PT_LOAD, 6, 0x5000, far, far, 0, 0x2000, 0x1000)
            for options in ((), ("--lazy-binding",)):
                result, output = relink(relinker, work, f"span-{far:x}{''.join(options)}", spanning, *options)
                if not accepted:
                    assert result.returncode == 2 and "Entry stub call exceeds rel32 range" in result.stderr and not output.exists(), (hex(far), options, result.returncode, result.stderr)
                    continue
                assert result.returncode == 0, (hex(far), options, result.stdout, result.stderr)
                if not options:
                    large_path = output
                    end = max(header[3] + header[6] for header in headers(output.read_bytes()) if header[0] == PT_LOAD)
                    assert BASE + far < end < GUEST_ARENA_START, ("the largest accepted image does not end below the guest arena", hex(end))

        filled = bytearray(relocated)
        filled[0x158:0x198] = b"\x41" * 0x40
        result, _ = relink(relinker, work, "relocated-filled", bytes(filled), "--linux-fixed-base", hex(BASE))
        assert result.returncode == 0, (result.stdout, result.stderr)

        if sys.platform.startswith("linux") and platform.machine() in ("x86_64", "AMD64"):
            fixed_path.chmod(0o755)
            executed = run_without_core_dump([str(fixed_path)], capture_output=True, timeout=20)
            assert executed.returncode == -signal.SIGTRAP, ("the image did not run at the base with its relocation applied", executed.returncode)
            relocated_fixed_path.chmod(0o755)
            executed = run_without_core_dump([str(relocated_fixed_path)], capture_output=True, timeout=20)
            assert executed.returncode == -signal.SIGTRAP, ("the relocated-table image did not run at the base with its relocation applied", executed.returncode)
            large_path.chmod(0o755)
            executed = run_without_core_dump([str(large_path)], capture_output=True, timeout=20)
            assert executed.returncode == -signal.SIGTRAP, ("the image spanning 2 GiB did not run at the base", executed.returncode)

        for name, image, base, error in (
            ("unaligned", fixture(alignment=0x20000), 0x410000, "is not a multiple of the PT_LOAD alignment 0x20000"),
            ("beyond-user-space", fixture(memory_size=0x100000), 0x7FFFFFFF0000, "outside the Linux user address space"),
            ("unsupported-type", fixture(relocation_type=11), BASE, "Unsupported relocation type 0xb"),
            ("negative-target", fixture(addend=-16), BASE, "Relocation target -16 lies below the image at a fixed Linux base"),
        ):
            result, output = relink(relinker, work, name, image, "--linux-fixed-base", hex(base))
            assert result.returncode == 2 and error in result.stderr and not output.exists(), (name, result.returncode, result.stdout, result.stderr)
    print("Linux fixed base test passed")


if __name__ == "__main__":
    main()
