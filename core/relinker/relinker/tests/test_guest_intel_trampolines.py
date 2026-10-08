from pathlib import Path
import struct
import subprocess
import sys
import tempfile

from test_linux_load_alignment import fixture as executable_fixture


SITE = bytes.fromhex("f2 0f 78 db 08 08")
SITE_ADDRESSES = (0x1002, 0x1012)
PLAIN_SITE = b"\x90" * len(SITE)
RIP_DISPLACEMENT = 0x10F6
RIP_SITE = bytes.fromhex("0f 38 cc 15") + struct.pack("<i", RIP_DISPLACEMENT)
CASES = (("plain", PLAIN_SITE), ("stub", SITE), ("rip", RIP_SITE))


def guest_fixture(site):
    image = bytearray(0xA00)
    image[:16] = b"\x7fELF\x02\x01\x01" + bytes(9)
    struct.pack_into("<HHIQQQIHHHHHH", image, 16,
                     3, 62, 1, 0x1000, 64, 0, 0, 64, 56, 3, 64, 0, 0)
    struct.pack_into("<IIQQQQQQ", image, 64,
                     1, 5, 0x400, 0x1000, 0x1000, 0x100, 0x100, 0x100)
    struct.pack_into("<IIQQQQQQ", image, 120,
                     1, 6, 0x600, 0x2000, 0x2000, 0x400, 0x400, 0x100)
    tags = [(5, 0x2200), (10, 1), (6, 0x2220), (11, 24),
            (4, 0x2240), (7, 0x2300), (8, 0), (9, 24), (0, 0)]
    struct.pack_into("<IIQQQQQQ", image, 176,
                     2, 6, 0x600, 0x2000, 0x2000, len(tags) * 16, len(tags) * 16, 8)
    image[0x400:0x500] = b"\x90" * 0x100
    code = bytes((0xEB, len(site))) + site + b"\xc3"
    image[0x400:0x400 + len(code)] = code
    image[0x410:0x410 + len(code)] = code
    for index, tag in enumerate(tags):
        struct.pack_into("<qQ", image, 0x600 + index * 16, *tag)
    struct.pack_into("<II", image, 0x840, 1, 1)
    return image


def main_fixture():
    image = executable_fixture()
    struct.pack_into("<Q", image, 24, 0x4000)
    struct.pack_into("<QQ", image, 64 + 16, 0x4000, 0x4000)
    struct.pack_into("<QQ", image, 120 + 16, 0x4600, 0x4600)
    for index in (0, 2, 4):
        address, = struct.unpack_from("<Q", image, 0x4600 + index * 16 + 8)
        struct.pack_into("<Q", image, 0x4600 + index * 16 + 8, address + 0x4000)
    return image


def elf_loads(data):
    offset, = struct.unpack_from("<Q", data, 32)
    size, count = struct.unpack_from("<HH", data, 54)
    headers = [struct.unpack_from("<IIQQQQQQ", data, offset + index * size)
               for index in range(count)]
    return [header for header in headers if header[0] == 1]


def elf_bytes_at(data, loads, address, size):
    for header in loads:
        _, flags, offset, mapped, _, file_size, _, _ = header
        if flags & 1 and mapped <= address and address + size <= mapped + file_size:
            start = offset + address - mapped
            return data[start:start + size]
    raise AssertionError(f"Executable ELF address is unmapped: {address:#x}")


def pe_sections(data):
    header, = struct.unpack_from("<I", data, 0x3C)
    count, = struct.unpack_from("<H", data, header + 6)
    optional_size, = struct.unpack_from("<H", data, header + 20)
    offset = header + 24 + optional_size
    sections = []
    for index in range(count):
        current = offset + index * 40
        name = data[current:current + 8].split(b"\0", 1)[0]
        _, rva, raw_size, raw_offset = struct.unpack_from("<IIII", data, current + 8)
        flags, = struct.unpack_from("<I", data, current + 36)
        sections.append((name, rva, raw_size, raw_offset, flags))
    return sections


def pe_bytes_at(data, sections, address, size):
    for _, rva, raw_size, raw_offset, flags in sections:
        if flags & 0x20000000 and rva <= address and address + size <= rva + raw_size:
            start = raw_offset + address - rva
            return data[start:start + size]
    raise AssertionError(f"Executable PE RVA is unmapped: {address:#x}")


def check_trampoline(read, site_address):
    patched = read(site_address, len(SITE))
    assert patched[0] == 0xE9 and patched[5] == 0x90, patched.hex()
    target = site_address + 5 + struct.unpack_from("<i", patched, 1)[0]
    body = read(target, 14)
    assert body[:5] == bytes.fromhex("66 0f 38 00 1d"), body.hex()
    assert body[9] == 0xE9, body.hex()
    continuation = target + 14 + struct.unpack_from("<i", body, 10)[0]
    assert continuation == site_address + len(SITE), (target, continuation)


def check_rip_trampoline(read, site_address):
    patched = read(site_address, len(RIP_SITE))
    assert patched[0] == 0xE9 and patched[5:] == b"\x90" * (len(RIP_SITE) - 5), patched.hex()
    target = site_address + 5 + struct.unpack_from("<i", patched, 1)[0]
    body = read(target, 21)
    assert body[13:17] == bytes.fromhex("f3 0f 6f 0d"), body.hex()
    operand = target + 21 + struct.unpack_from("<i", body, 17)[0]
    assert operand == site_address + len(RIP_SITE) + RIP_DISPLACEMENT, (site_address, operand)


def main():
    relinker = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="anyps5-guest-intel-") as directory:
        work = Path(directory)
        for windows in (False, True):
            for kind, site in CASES:
                has_stub = kind != "plain"
                case = work / ("windows" if windows else "linux") / kind
                module_dir = case / "sce_module"
                module_dir.mkdir(parents=True)
                source = case / "input.elf"
                output = case / ("output.exe" if windows else "output.elf")
                source.write_bytes(main_fixture())
                (module_dir / "sample.prx").write_bytes(guest_fixture(site))
                arguments = [str(relinker), "--to-intel"]
                if windows:
                    arguments.append("--windows")
                result = subprocess.run(arguments + [str(source), str(output)],
                                        capture_output=True, text=True, timeout=30)
                assert result.returncode == 0, (windows, has_stub, result.stdout, result.stderr)
                module = case / "app0" / "sce_module" / "sample.prx.guest.prx"
                data = module.read_bytes()
                if windows:
                    assert data[:2] == b"MZ", (windows, has_stub)
                    sections = pe_sections(data)
                    code = next(section for section in sections if section[0] == b".elf0")
                    site_addresses = (code[1] + 2, code[1] + 0x12)
                    read = lambda address, size: pe_bytes_at(data, sections, address, size)
                    assert any(section[0] == b".amdstub" for section in sections) == has_stub
                else:
                    assert data[:4] == b"\x7fELF", (windows, has_stub)
                    loads = elf_loads(data)
                    site_addresses = SITE_ADDRESSES
                    read = lambda address, size: elf_bytes_at(data, loads, address, size)
                if kind == "rip":
                    for site_address in site_addresses:
                        check_rip_trampoline(read, site_address)
                elif has_stub:
                    for site_address in site_addresses:
                        check_trampoline(read, site_address)
                else:
                    for site_address in site_addresses:
                        assert read(site_address, len(SITE)) == PLAIN_SITE
    print("Guest Intel trampoline integration tests passed")


if __name__ == "__main__":
    main()
