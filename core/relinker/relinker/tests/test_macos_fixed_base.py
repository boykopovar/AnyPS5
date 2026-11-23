"""Check that --macos-fixed-base places the lowest ELF load page at the requested address and rebases every address the Mach-O holds."""

from pathlib import Path
import hashlib
import struct
import subprocess
import sys
import tempfile

from test_linux_fixed_base import ENTRY, SLOT, fixture
from test_linux_load_alignment import PT_SCE_VERSION
from test_macos_output import IMAGE_BASE, MachO

BASE = 0x140010000
LOAD_RVA = 0x10000


def relink(relinker, directory, name, image, *options):
    source = directory / (name + ".elf")
    output = directory / name
    source.write_bytes(image)
    result = subprocess.run([str(relinker), "--macos", "--skip-sce-module", *options, str(source), str(output)],
                            capture_output=True, text=True, timeout=20)
    return result, output


def many_exports(count):
    strings = bytearray(b"\0")
    symbols = bytearray(24)
    for index in range(count):
        name = hashlib.sha256(index.to_bytes(4, "little")).hexdigest()[:32]
        symbols += struct.pack("<IBBHQQ", len(strings), 0x12, 0, 1, 0x210, 6)
        strings += ("Export" + name + "#A#B").encode() + b"\0"
    symtab = 0x1000 + (len(strings) + 7) // 8 * 8
    size = (symtab + len(symbols) + 0xFFF) // 0x1000 * 0x1000
    image = bytearray(0x4000 + size)
    image[:16] = b"\x7fELF\x02\x01\x01" + bytes(9)
    struct.pack_into("<HHIQQQIHHHHHH", image, 16, 3, 62, 1, 0x210, 64, 0, 0, 64, 56, 6, 64, 0, 0)
    tags = [(5, 0x1000), (10, len(strings)), (6, symtab), (11, 24), (7, 0x700), (8, 0), (9, 24), (0x6100003F, len(symbols)), (0, 0)]
    struct.pack_into("<IIQQQQQQ", image, 64, 1, 5, 0x4000, 0, 0, size, size, 0x4000)
    struct.pack_into("<IIQQQQQQ", image, 120, 2, 6, 0x4400, 0x400, 0x400, len(tags) * 16, len(tags) * 16, 8)
    for index in range(2, 6):
        struct.pack_into("<IIQQQQQQ", image, 64 + index * 56, PT_SCE_VERSION, 0, 0, 0, 0, 0, 0, 1)
    for index, tag in enumerate(tags):
        struct.pack_into("<qQ", image, 0x4400 + index * 16, *tag)
    image[0x4210:0x4216] = bytes.fromhex("b82a000000c3")
    image[0x5000:0x5000 + len(strings)] = strings
    image[0x4000 + symtab:0x4000 + symtab + len(symbols)] = symbols
    return image


def check_large_exports(name, macho, base):
    text = macho.segment("__TEXT")
    linkedit = macho.segment("__LINKEDIT")
    low = macho.segment("__APS5LOW")
    start, size = macho.dyld_info[8:10]
    assert size > 0x10000, f"{name}: the export trie holds only {size:#x} bytes"
    assert first_load(macho)["vmaddr"] == base, f"{name}: the base was not applied"
    assert linkedit["vmaddr"] + linkedit["vmsize"] == 0x100000000, f"{name}: __LINKEDIT does not end at 0x100000000: {linkedit}"
    assert low["vmaddr"] + low["vmsize"] == linkedit["vmaddr"], f"{name}: the low reserve does not reach __LINKEDIT: {low}"
    end = start + size - linkedit["fileoff"] + linkedit["vmaddr"] - text["vmaddr"]
    assert end <= 0xFFFFFFFF, f"{name}: the export trie ends {end:#x} past __TEXT, beyond dyld's 32-bit runtime offset"


def first_load(macho):
    return next(segment for segment in macho.segments if segment["name"].startswith("__ELF"))


def image_segments(macho):
    return [segment for segment in macho.segments[1:] if segment["name"] not in ("__APS5LOW", "__LINKEDIT")]


def check_layout(plain, fixed):
    shift = BASE - LOAD_RVA - IMAGE_BASE
    assert [s["name"] for s in image_segments(plain)] == [s["name"] for s in image_segments(fixed)], "segment list changed"
    assert "__APS5LOW" in [s["name"] for s in plain.segments], "the default base reserves no low address range"
    assert "__APS5LOW" not in [s["name"] for s in fixed.segments], "an image above 0x140000000 reserves a low address range"
    zero = fixed.segment("__PAGEZERO")
    assert zero["vmaddr"] == 0 and zero["vmsize"] == BASE - LOAD_RVA, f"__PAGEZERO {zero}"
    for before, after in zip(image_segments(plain), image_segments(fixed)):
        expected = dict(before, vmaddr=before["vmaddr"] + shift)
        assert after == expected, (before, after)
    for key, (address, size, offset) in plain.sections.items():
        assert fixed.sections[key] == (address + shift, size, offset), (key, fixed.sections[key])
    assert fixed.segment("__TEXT")["vmaddr"] == BASE - LOAD_RVA, "the Mach-O header is not 64 KiB below the base"
    assert first_load(fixed)["vmaddr"] == BASE, "the lowest load page is not at the base"
    assert fixed.entryoff < LOAD_RVA and plain.entryoff < LOAD_RVA, "the entry point left the Mach-O header page"


def check_addresses(name, macho, data, base):
    _, _, offset = macho.sections[("__APS5DATA", "__meta")]
    load_bias = struct.unpack_from("<Q", data, offset + 8)[0]
    assert load_bias == base, f"{name}: load bias {load_bias:#x}, expected {base:#x}"
    elf = first_load(macho)
    slot = struct.unpack_from("<Q", data, elf["fileoff"] + SLOT)[0]
    assert slot == base + ENTRY, f"{name}: relocated slot holds {slot:#x}, expected {base + ENTRY:#x}"


def main():
    relinker = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="anyps5-macos-fixed-base-") as directory:
        work = Path(directory)
        result, plain_path = relink(relinker, work, "plain", fixture())
        assert result.returncode == 0, (result.stdout, result.stderr)
        result, fixed_path = relink(relinker, work, "fixed", fixture(), "--macos-fixed-base", hex(BASE))
        assert result.returncode == 0, (result.stdout, result.stderr)
        plain_data = plain_path.read_bytes()
        fixed_data = fixed_path.read_bytes()
        plain = MachO(plain_data)
        fixed = MachO(fixed_data)

        check_layout(plain, fixed)
        check_addresses("plain", plain, plain_data, IMAGE_BASE + LOAD_RVA)
        check_addresses("fixed", fixed, fixed_data, BASE)

        lowest, lowest_path = relink(relinker, work, "lowest", fixture(), "--macos-fixed-base", "0x20000")
        assert lowest.returncode == 0, (lowest.stdout, lowest.stderr)
        assert first_load(MachO(lowest_path.read_bytes()))["vmaddr"] == 0x20000, "the lowest base was not applied"

        large = many_exports(2500)
        for name, options, base in (("large-default", [], IMAGE_BASE + LOAD_RVA), ("large-lowest", ["--macos-fixed-base", "0x20000"], 0x20000)):
            result, output = relink(relinker, work, name, large, *options)
            assert result.returncode == 0, (name, result.stdout, result.stderr)
            check_large_exports(name, MachO(output.read_bytes()), base)

        for name, options, error in (
            ("unaligned", ["--macos-fixed-base", "0x140018000"], "--macos-fixed-base requires a 64 KiB-aligned address from 0x20000 through 0x7fffffff0000"),
            ("below", ["--macos-fixed-base", "0x10000"], "--macos-fixed-base requires a 64 KiB-aligned address from 0x20000 through 0x7fffffff0000"),
            ("missing", ["--macos-fixed-base", "--registry"], "--macos-fixed-base requires an address"),
            ("repeated", ["--macos-fixed-base", hex(BASE), "--macos-fixed-base", hex(BASE)], "--macos-fixed-base cannot be repeated"),
            ("beyond-user-space", ["--macos-fixed-base", "0x7fffffff0000"], "Mach-O image ends outside the macOS user address space"),
        ):
            result, output = relink(relinker, work, name, fixture(), *options)
            assert result.returncode != 0 and error in result.stdout + result.stderr and not output.exists(), (name, result.returncode, result.stdout, result.stderr)

        source = work / "linux.elf"
        source.write_bytes(fixture())
        result = subprocess.run([str(relinker), "--skip-sce-module", "--macos-fixed-base", hex(BASE), str(source), str(work / "linux.out")],
                                capture_output=True, text=True, timeout=20)
        assert result.returncode != 0 and "--macos-fixed-base requires --macos" in result.stdout + result.stderr, (result.stdout, result.stderr)
    print("macOS fixed base test passed")


if __name__ == "__main__":
    main()
