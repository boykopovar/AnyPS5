from pathlib import Path
import struct
import subprocess
import sys
import tempfile

from test_guest_intel_trampolines import elf_bytes_at, elf_loads, guest_fixture, main_fixture, pe_bytes_at, pe_sections


GENERIC_STUB = bytes.fromhex(
    "48 89 f8 48 89 f7 48 89 d6 48 89 ca 4d 89 c2 4d 89 c8 "
    "4c 8b 4c 24 08 0f 05 c3")
NUMBERED_STUB = bytes.fromhex("48 c7 c0 09 00 00 00 49 89 ca 0f 05 c3")
MPROTECT_INLINE = bytes.fromhex(
    "55 48 89 e5 53 50 89 d1 49 89 f0 48 89 fe f6 c1 04 75 31 48 63 d1 "
    "b8 4a 00 00 00 48 89 f7 4c 89 c6 45 31 d2 45 31 c0 45 31 c9 "
    "0f 05 0f 93 c1 48 89 c3 48 f7 db 84 c9 48 0f 45 d8 48 85 db")
MMAP_INLINE = bytes.fromhex(
    "49 63 d6 4c 63 d1 4d 63 c0 b8 dd 01 00 00 48 89 de "
    "0f 05 48 89 c1 0f 93 c2 48 f7 d8 84 d2 48 0f 45 c1 48 85 c0")
MMAP_EXEC_INLINE = bytes.fromhex(
    "44 89 f0 83 e0 fb 48 63 d0 4c 63 d1 4d 63 c0 b8 dd 01 00 00 48 89 de "
    "0f 05 0f 93 c1 49 89 c7 49 f7 df 84 c9 4c 0f 45 f8 4d 85 ff")


def main():
    relinker = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="anyps5-guest-syscall-trap-") as directory:
        work = Path(directory)
        for windows in (False, True):
            case = work / ("windows" if windows else "linux")
            modules = case / "sce_module"
            modules.mkdir(parents=True)
            module = bytearray(guest_fixture(b"\x90" * 6))
            for offset, stub in ((0x400, GENERIC_STUB), (0x440, NUMBERED_STUB),
                                 (0x450, MPROTECT_INLINE), (0x490, MMAP_INLINE), (0x4b8, MMAP_EXEC_INLINE)):
                module[offset:offset + len(stub)] = stub
            (modules / "sample.prx").write_bytes(module)
            source = case / "input.elf"
            source.write_bytes(main_fixture())
            output = case / ("output.exe" if windows else "output.elf")
            options = ["--windows"] if windows else []
            result = subprocess.run(
                [str(relinker), *options, "--trap-sdk-syscall-stubs", str(source), str(output)],
                capture_output=True, text=True, timeout=30)
            assert result.returncode == 0, (windows, result.stdout, result.stderr)

            converted = output.parent / "app0" / "sce_module" / "sample.prx.guest.prx"
            image = converted.read_bytes()
            if windows:
                sections = pe_sections(image)
                base = next(section[1] for section in sections if section[0] == b".elf0")
                converted_code = pe_bytes_at(image, sections, base, 0x100)
            else:
                converted_code = elf_bytes_at(image, elf_loads(image), 0x1000, 0x100)
            expected = bytearray(module[0x400:0x500])
            for offset in (0x17, 0x4a, 0x7a, 0xa1, 0xcf):
                assert expected[offset:offset + 2] == b"\x0f\x05"
                expected[offset:offset + 2] = b"\x0f\x0b"
            assert converted_code == expected, (windows, converted_code.hex(), expected.hex())


if __name__ == "__main__":
    main()
