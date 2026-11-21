"""Check that --macos-fixed-base places the lowest ELF load page at the requested address and rebases every address the Mach-O holds."""

from pathlib import Path
import struct
import subprocess
import sys
import tempfile

from test_linux_fixed_base import ENTRY, SLOT, fixture
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
