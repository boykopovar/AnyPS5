"""Route ps5-payload-dev pathconf numbered syscall stub to the libkernel export."""

from pathlib import Path
import struct
import subprocess
import sys
import tempfile

from test_guest_intel_trampolines import elf_bytes_at, elf_loads, pe_bytes_at, pe_sections


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


def main():
    relinker = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="anyps5-syscall-route-") as directory:
        work = Path(directory)

        # 1. Successful routing for Linux and Windows targets without --trap-sdk-syscall-stubs
        for symbol in (b"pathconf", b"oc98+zOImus"):
            for windows in (False, True):
                case_name = f"{symbol.decode()}_{'win' if windows else 'linux'}"
                source = work / f"{case_name}.elf"
                output = work / (f"{case_name}.exe" if windows else f"{case_name}.out")
                source.write_bytes(fixture(STUB_191, import_symbol=symbol))

                options = ["--skip-sce-module"]
                if windows:
                    options.append("--windows")

                result = subprocess.run(
                    [str(relinker), *options, str(source), str(output)],
                    capture_output=True, text=True, timeout=20,
                )
                assert result.returncode == 0, (case_name, result.stdout, result.stderr)

                relinked = output.read_bytes()
                if windows:
                    sections = pe_sections(relinked)
                    code_sec = next(s for s in sections if s[0].startswith(b".elf") and (s[4] & 0x20000000))
                    stub_bytes = pe_bytes_at(relinked, sections, code_sec[1], 13)
                else:
                    loads = elf_loads(relinked)
                    code_load = next(header for header in loads if (header[1] & 1))
                    stub_bytes = elf_bytes_at(relinked, loads, code_load[3], 13)

                # Expect: FF 25 <disp32> followed by 7 NOPs (0x90)
                assert stub_bytes[0:2] == b"\xff\x25", (case_name, stub_bytes.hex())
                assert stub_bytes[6:13] == b"\x90" * 7, (case_name, stub_bytes.hex())
                disp32 = struct.unpack_from("<i", stub_bytes, 2)[0]
                # Target of FF 25 at 0x1000 is 0x1000 + 6 + disp32 == 0x800
                assert 0x1000 + 6 + disp32 == 0x800, (case_name, hex(disp32))

        # 2. Syscall 191 without pathconf import fails without --trap-sdk-syscall-stubs
        no_import_source = work / "no_import.elf"
        no_import_out = work / "no_import.out"
        no_import_source.write_bytes(fixture(STUB_191, import_symbol=None))
        result = subprocess.run(
            [str(relinker), "--skip-sce-module", str(no_import_source), str(no_import_out)],
            capture_output=True, text=True, timeout=20,
        )
        assert result.returncode != 0, "Expected failure when pathconf import is missing"
        assert "Homebrew ps5-payload-dev syscall stub 191 cannot be routed: missing libkernel pathconf import" in result.stderr, result.stderr

        # 3. Syscall 191 without pathconf import falls back to UD2 trap when --trap-sdk-syscall-stubs is set
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

        # 4. Syscall 9 (unrouted number) fails without --trap-sdk-syscall-stubs
        unrouted_source = work / "unrouted.elf"
        unrouted_out = work / "unrouted.out"
        unrouted_source.write_bytes(fixture(STUB_9, import_symbol=b"pathconf"))
        result = subprocess.run(
            [str(relinker), "--skip-sce-module", str(unrouted_source), str(unrouted_out)],
            capture_output=True, text=True, timeout=20,
        )
        assert result.returncode != 0, "Expected failure for unrouted syscall number 9"
        assert "Forbidden syscall instruction" in result.stderr, result.stderr

    print("Guest syscall route tests passed")


if __name__ == "__main__":
    main()
