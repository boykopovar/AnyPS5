"""Check that --to-intel stubs are written into guest modules instead of failing the relink."""

from pathlib import Path
import struct
import subprocess
import sys
import tempfile

from test_linux_load_alignment import fixture as linux_executable
from test_optional_plt import fixture

EXTRQ_REGISTER_FORM = b"\x66\x0f\x79\xca"
SITE = 0x220


def guest(instruction=EXTRQ_REGISTER_FORM):
    image = fixture(((4, 0x680),))
    struct.pack_into("<IIIII", image, 0x680, 1, 2, 0, 0, 0)
    image[-1] = 0x90
    image[SITE:SITE + len(instruction)] = instruction
    image[SITE + len(instruction)] = 0x90
    image[SITE + len(instruction) + 1] = 0xC3
    return image


def segments(elf):
    count = struct.unpack_from("<H", elf, 56)[0]
    offset = struct.unpack_from("<Q", elf, 32)[0]
    return [struct.unpack_from("<IIQQQQQQ", elf, offset + index * 56) for index in range(count)]


def relink(relinker, work, name, windows):
    root = work / name
    (root / "sce_module").mkdir(parents=True)
    (root / "input.elf").write_bytes(fixture() if windows else linux_executable())
    (root / "sce_module" / "module").write_bytes(guest())
    output = root / ("output.exe" if windows else "output.elf")
    command = [str(relinker), "--to-intel"] + (["--windows"] if windows else []) + [str(root / "input.elf"), str(output)]
    result = subprocess.run(command, capture_output=True, text=True, timeout=60)
    assert result.returncode == 0, (name, result.stdout, result.stderr)
    modules = list(output.parent.rglob("*.guest.prx"))
    assert len(modules) == 1, (name, modules)
    return modules[0].read_bytes(), result.stdout


def main():
    relinker = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="anyps5-guest-intel-") as directory:
        work = Path(directory)

        module, output = relink(relinker, work, "linux", windows=False)
        assert "module.guest.prx" in output, output
        assert module[SITE] == 0xE9, "the guest site was not replaced by a jump"
        displacement = struct.unpack_from("<i", module, SITE + 1)[0]
        loads = [header for header in segments(module) if header[0] == 1]
        assert any(header[1] & 1 for header in loads), "the guest module has no executable segment"

        target = SITE + 5 + displacement
        stub = None
        for _, flags, offset, vaddr, _, filesz, _, _ in loads:
            if vaddr <= target < vaddr + filesz:
                assert flags & 1, "the guest stub is not in an executable segment"
                stub = offset + target - vaddr
        assert stub is not None, f"the jump leaves the guest image: {target:#x}"
        assert module[stub] == 0x48, "the stub does not start with the red-zone skip"
        assert 0xE9 in module[stub:stub + 0x200], "the stub has no return jump"

        windows_module, windows_output = relink(relinker, work, "windows", windows=True)
        assert "module.guest.prx" in windows_output, windows_output
        assert windows_module[:2] == b"MZ", "the Windows guest module is not a PE"
        pe = struct.unpack_from("<I", windows_module, 0x3c)[0]
        count = struct.unpack_from("<H", windows_module, pe + 6)[0]
        optional = struct.unpack_from("<H", windows_module, pe + 20)[0]
        names = [windows_module[pe + 24 + optional + index * 40:][:8].rstrip(b"\0").decode() for index in range(count)]
        assert ".amdstub" in names, names
    print("Guest module --to-intel stub tests passed")


if __name__ == "__main__":
    main()
