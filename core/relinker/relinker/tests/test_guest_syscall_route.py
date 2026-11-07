"""Route ps5-payload-dev pathconf numbered syscall stub to the libkernel export."""

from pathlib import Path
import struct
import subprocess
import sys
import tempfile

from test_guest_intel_trampolines import (
    elf_bytes_at,
    elf_loads,
    guest_fixture,
    main_fixture,
    pe_bytes_at,
    pe_sections,
)
from test_macos_output import MachO


STUB_191 = bytes.fromhex("48 c7 c0 bf 00 00 00 49 89 ca 0f 05 c3")
STUB_9 = bytes.fromhex("48 c7 c0 09 00 00 00 49 89 ca 0f 05 c3")


def fixture(stub_code, import_symbol=b"pathconf", import_library=b"libkernel.prx"):
    code_base = 0x1000
    slot = 0x800
    image = bytearray(0x1200)
    image[:16] = b"\x7fELF\x02\x01\x01" + bytes(9)
    struct.pack_into("<HHIQQQIHHHHHH", image, 16, 3, 62, 1, code_base, 64, 0, 0, 64, 56, 8, 64, 0, 0)
    struct.pack_into("<IIQQQQQQ", image, 64, 1, 6, 0, 0, 0, 0x1000, 0x1000, 0x1000)
    struct.pack_into("<IIQQQQQQ", image, 120, 2, 6, 0x400, 0x400, 0x400, 0x100, 0x100, 8)
    struct.pack_into("<IIQQQQQQ", image, 176, 1, 5, code_base, code_base, code_base, 0x200, 0x200, 0x1000)
    for idx in range(3, 8):
        struct.pack_into("<IIQQQQQQ", image, 64 + idx * 56, 0x6fffff01, 0, 0, 0, 0, 0, 0, 1)

    if import_symbol is not None:
        relocations = [(slot, (1 << 32) | 7, 0)]
        strtab = b"\x00" + import_symbol + b"\x00" + import_library + b"\x00"
        lib_offset = 1 + len(import_symbol) + 1
        tags = [
            (1, lib_offset),
            (5, 0x600),
            (10, len(strtab)),
            (6, 0x620),
            (11, 24),
            (3, slot),
            (2, 24),
            (20, 7),
            (23, 0x700),
            (7, 0x7a0),
            (8, 0),
            (9, 24),
            (0x6100003f, 48),
            (0, 0),
        ]
        image[0x600:0x600 + len(strtab)] = strtab
        struct.pack_into("<IBBHQQ", image, 0x620 + 24, 1, (1 << 4) | 2, 0, 0, 0, 0)
        for idx, relocation in enumerate(relocations):
            struct.pack_into("<QQq", image, 0x700 + idx * 24, *relocation)
    else:
        tags = [
            (5, 0x600),
            (10, 1),
            (6, 0x620),
            (11, 24),
            (7, 0x700),
            (8, 0),
            (9, 24),
            (0, 0),
        ]

    for idx, tag in enumerate(tags):
        struct.pack_into("<qQ", image, 0x400 + idx * 16, *tag)

    image[code_base:code_base + len(stub_code)] = stub_code
    return bytes(image)


def guest_module_fixture(stub_code, import_symbol=b"pathconf", import_library=b"libkernel.prx"):
    image = bytearray(guest_fixture(b"\x90" * 6))
    image[0x440:0x440 + len(stub_code)] = stub_code
    if import_symbol is not None:
        strtab = b"\x00" + import_symbol + b"\x00" + import_library + b"\x00"
        image[0x800:0x800 + len(strtab)] = strtab
        struct.pack_into("<IBBHQQ", image, 0x820 + 24, 1, 0x12, 0, 0, 0, 0)
        struct.pack_into("<II", image, 0x840, 1, 2)
        slot = 0x2050
        rela_offset = 0x8C0
        rela_mapped = 0x22C0
        struct.pack_into("<QQq", image, rela_offset, slot, (1 << 32) | 7, 0)
        tags = [
            (1, 1 + len(import_symbol) + 1),
            (5, 0x2200),
            (10, len(strtab)),
            (6, 0x2220),
            (11, 24),
            (4, 0x2240),
            (0x6100003f, 48),
            (23, rela_mapped),
            (2, 24),
            (20, 7),
            (7, rela_mapped + 24),
            (8, 0),
            (9, 24),
            (0, 0),
        ]
        struct.pack_into("<IIQQQQQQ", image, 176, 2, 6, 0x600, 0x2000, 0x2000, len(tags) * 16, len(tags) * 16, 8)
        for idx, tag in enumerate(tags):
            struct.pack_into("<qQ", image, 0x600 + idx * 16, *tag)
    return bytes(image)


def main():
    relinker = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="anyps5-syscall-route-") as directory:
        work = Path(directory)

        for symbol in (b"pathconf", b"oc98+zOImus"):
            for platform in ("linux", "windows", "macos"):
                case_name = f"{symbol.decode()}_{platform}"
                source = work / f"{case_name}.elf"
                output = work / f"{case_name}.out"
                source.write_bytes(fixture(STUB_191, import_symbol=symbol))

                options = ["--skip-sce-module"]
                if platform == "windows":
                    options.append("--windows")
                elif platform == "macos":
                    options.append("--macos")

                result = subprocess.run(
                    [str(relinker), *options, str(source), str(output)],
                    capture_output=True, text=True, timeout=20,
                )
                assert result.returncode == 0, (case_name, result.stdout, result.stderr)

                relinked = output.read_bytes()
                if platform == "windows":
                    sections = pe_sections(relinked)
                    code_sec = next(s for s in sections if s[0].startswith(b".elf") and (s[4] & 0x20000000))
                    stub_bytes = pe_bytes_at(relinked, sections, code_sec[1], 13)
                elif platform == "macos":
                    macho = MachO(relinked)
                    seg = next(s for s in macho.segments if s["name"] == "__ELF1")
                    stub_bytes = relinked[seg["fileoff"]:seg["fileoff"] + 13]
                else:
                    loads = elf_loads(relinked)
                    code_load = next(header for header in loads if (header[1] & 1))
                    stub_bytes = elf_bytes_at(relinked, loads, code_load[3], 13)

                assert stub_bytes[0:2] == b"\xff\x25", (case_name, stub_bytes.hex())
                assert stub_bytes[6:13] == b"\x90" * 7, (case_name, stub_bytes.hex())
                disp32 = struct.unpack_from("<i", stub_bytes, 2)[0]
                assert 0x1000 + 6 + disp32 == 0x800, (case_name, hex(disp32))

        no_import_source = work / "no_import.elf"
        no_import_out = work / "no_import.out"
        no_import_source.write_bytes(fixture(STUB_191, import_symbol=None))
        result = subprocess.run(
            [str(relinker), "--skip-sce-module", str(no_import_source), str(no_import_out)],
            capture_output=True, text=True, timeout=20,
        )
        assert result.returncode != 0, "Expected failure when pathconf import is missing"
        assert "Homebrew ps5-payload-dev syscall stub 191 cannot be routed: missing libkernel pathconf import" in result.stderr, result.stderr

        trap_out = work / "trapped.out"
        result = subprocess.run(
            [str(relinker), "--skip-sce-module", "--trap-sdk-syscall-stubs", str(no_import_source), str(trap_out)],
            capture_output=True, text=True, timeout=20,
        )
        assert result.returncode == 0, (result.stdout, result.stderr)
        trapped_loads = elf_loads(trap_out.read_bytes())
        trapped_code_load = next(header for header in trapped_loads if (header[1] & 1))
        trapped_bytes = elf_bytes_at(trap_out.read_bytes(), trapped_loads, trapped_code_load[3], 13)
        assert trapped_bytes[10:12] == b"\x0f\x0b", trapped_bytes.hex()

        unrouted_source = work / "unrouted.elf"
        unrouted_out = work / "unrouted.out"
        unrouted_source.write_bytes(fixture(STUB_9, import_symbol=b"pathconf"))
        result = subprocess.run(
            [str(relinker), "--skip-sce-module", str(unrouted_source), str(unrouted_out)],
            capture_output=True, text=True, timeout=20,
        )
        assert result.returncode != 0, "Expected failure for unrouted syscall number 9"
        assert "Forbidden syscall instruction" in result.stderr, result.stderr

        for symbol in (b"pathconf", b"oc98+zOImus"):
            for platform in ("linux", "windows", "macos"):
                case_dir = work / f"guest_{symbol.decode()}_{platform}"
                mods_dir = case_dir / "sce_module"
                mods_dir.mkdir(parents=True)
                (mods_dir / "sample.prx").write_bytes(guest_module_fixture(STUB_191, import_symbol=symbol))

                source = case_dir / "input.elf"
                source.write_bytes(main_fixture())
                output = case_dir / f"output_{platform}.bin"
                options = []
                if platform == "windows":
                    options.append("--windows")
                elif platform == "macos":
                    options.append("--macos")

                result = subprocess.run(
                    [str(relinker), *options, str(source), str(output)],
                    capture_output=True, text=True, timeout=20,
                )
                assert result.returncode == 0, (result.stdout, result.stderr)

                converted = output.parent / "app0" / "sce_module" / "sample.prx.guest.prx"
                image = converted.read_bytes()
                if platform == "windows":
                    sections = pe_sections(image)
                    base = next(section[1] for section in sections if section[0] == b".elf0")
                    stub_bytes = pe_bytes_at(image, sections, base + 0x40, 13)
                elif platform == "macos":
                    macho = MachO(image)
                    seg0 = next(s for s in macho.segments if s["name"] == "__ELF0")
                    stub_bytes = image[seg0["fileoff"] + 0x40:seg0["fileoff"] + 0x40 + 13]
                else:
                    loads = elf_loads(image)
                    stub_bytes = elf_bytes_at(image, loads, 0x1040, 13)

                assert stub_bytes[0:2] == b"\xff\x25", stub_bytes.hex()
                assert stub_bytes[6:13] == b"\x90" * 7, stub_bytes.hex()
                disp32 = struct.unpack_from("<i", stub_bytes, 2)[0]
                assert 0x1040 + 6 + disp32 == 0x2050, (hex(disp32), stub_bytes.hex())

        guest_no_import_dir = work / "guest_no_import"
        mods_dir = guest_no_import_dir / "sce_module"
        mods_dir.mkdir(parents=True)
        (mods_dir / "sample.prx").write_bytes(guest_module_fixture(STUB_191, import_symbol=None))
        source = guest_no_import_dir / "input.elf"
        source.write_bytes(main_fixture())
        output = guest_no_import_dir / "output.elf"
        result = subprocess.run(
            [str(relinker), str(source), str(output)],
            capture_output=True, text=True, timeout=20,
        )
        assert result.returncode != 0, "Expected failure when guest module lacks pathconf import"
        assert "Homebrew ps5-payload-dev syscall stub 191 cannot be routed: missing libkernel pathconf import" in result.stderr, result.stderr

        output_trapped = guest_no_import_dir / "output_trapped.elf"
        result = subprocess.run(
            [str(relinker), "--trap-sdk-syscall-stubs", str(source), str(output_trapped)],
            capture_output=True, text=True, timeout=20,
        )
        assert result.returncode == 0, (result.stdout, result.stderr)
        converted = output_trapped.parent / "app0" / "sce_module" / "sample.prx.guest.prx"
        image = converted.read_bytes()
        loads = elf_loads(image)
        stub_bytes = elf_bytes_at(image, loads, 0x1040, 13)
        assert stub_bytes[10:12] == b"\x0f\x0b", stub_bytes.hex()

    print("Guest syscall route tests passed")


if __name__ == "__main__":
    main()
