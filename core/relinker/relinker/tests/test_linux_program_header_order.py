"""Check that a Linux relink orders PT_PHDR and PT_INTERP before the PT_LOAD entries and sorts the PT_LOAD entries by address."""

from pathlib import Path
import platform
import signal
import struct
import subprocess
import sys
import tempfile

from test_linux_load_alignment import fixture as single_load_fixture

PT_LOAD = 1
PT_DYNAMIC = 2
PT_INTERP = 3
PT_PHDR = 6
PT_OS_PROCPARAM = 0x61000001
PT_SCE_VERSION = 0x6FFFFF01
PAGE = 0x1000

TEXT = (PT_LOAD, 5, 0x4000, 0x0000, 0x1000, 0x1000, 0x1000)
RODATA = (PT_LOAD, 4, 0x5000, 0x1000, 0x0800, 0x0800, 0x1000)
DATA = (PT_LOAD, 6, 0x6000, 0x2000, 0x1000, 0x1800, 0x1000)
PROCPARAM = (PT_OS_PROCPARAM, 4, 0x5400, 0x1400, 0x0040, 0x0040, 0x10)
DYNAMIC_OFFSET = 0x6400
TAGS = [(5, 0x2600), (10, 1), (6, 0x2620), (11, 24), (7, 0x2700), (8, 0), (9, 24), (0, 0)]


def multi_load_fixture(segments):
    image = bytearray(0x8000)
    image[:16] = b"\x7fELF\x02\x01\x01" + bytes(9)
    headers = [*segments, (PT_DYNAMIC, 6, DYNAMIC_OFFSET, 0x2400, len(TAGS) * 16, len(TAGS) * 16, 8)]
    headers += [(PT_SCE_VERSION, 0, 0, 0, 0, 0, 1)] * 4
    struct.pack_into("<HHIQQQIHHHHHH", image, 16,
                     3, 62, 1, TEXT[3], 64, 0, 0, 64, 56, len(headers), 64, 0, 0)
    image[TEXT[2]:TEXT[2] + 1] = b"\xcc"
    for index, (kind, flags, offset, vaddr, filesz, memsz, align) in enumerate(headers):
        struct.pack_into("<IIQQQQQQ", image, 64 + index * 56,
                         kind, flags, offset, vaddr, vaddr, filesz, memsz, align)
    for index, tag in enumerate(TAGS):
        struct.pack_into("<qQ", image, DYNAMIC_OFFSET + index * 16, *tag)
    return image


def program_headers(elf):
    phoff, = struct.unpack_from("<Q", elf, 0x20)
    phentsize, phnum = struct.unpack_from("<HH", elf, 0x36)
    assert phoff + phnum * phentsize <= len(elf), (phoff, phnum, phentsize, len(elf))
    return [struct.unpack_from("<IIQQQQQQ", elf, phoff + index * phentsize) for index in range(phnum)]


def check_order(name, elf):
    headers = program_headers(elf)
    kinds = [header[0] for header in headers]
    loads = [header for header in headers if header[0] == PT_LOAD]
    assert kinds[0] == PT_PHDR, (name, kinds)
    assert kinds.count(PT_PHDR) == 1 and kinds.count(PT_INTERP) == 1 and kinds.count(PT_DYNAMIC) == 1, (name, kinds)
    assert kinds.index(PT_INTERP) < kinds.index(PT_LOAD), (name, kinds)
    addresses = [header[3] for header in loads]
    assert addresses == sorted(addresses) and len(set(addresses)) == len(addresses), (name, [hex(a) for a in addresses])
    for header in loads:
        assert header[3] % header[7] == header[2] % header[7], (name, header)
    table_order_span = loads[-1][3] + loads[-1][6] - (loads[0][3] & ~(PAGE - 1))
    address_span = max(h[3] + h[6] for h in loads) - (min(h[3] for h in loads) & ~(PAGE - 1))
    assert table_order_span == address_span, (name, hex(table_order_span), hex(address_span))
    phdr = headers[0]
    phoff, = struct.unpack_from("<Q", elf, 0x20)
    assert phdr[2] == phoff, (name, phdr)
    assert any(h[3] <= phdr[3] and phdr[3] + phdr[5] <= h[3] + h[5] for h in loads), (name, phdr, loads)
    return headers


def run_expecting_trap(name, output):
    if not (sys.platform.startswith("linux") and platform.machine() in ("x86_64", "AMD64")):
        return
    output.chmod(0o755)
    executed = subprocess.run([str(output)], capture_output=True, timeout=20)
    assert executed.returncode == -signal.SIGTRAP, (name, executed.returncode, executed.stderr)


def main():
    relinker = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="anyps5-phdr-order-") as directory:
        work = Path(directory)

        def convert(name, image):
            source = work / (name + ".elf")
            output = work / (name + ".out.elf")
            source.write_bytes(image)
            result = subprocess.run([str(relinker), "--skip-sce-module", str(source), str(output)],
                                    capture_output=True, text=True, timeout=20)
            assert result.returncode == 0, (name, result.stdout, result.stderr)
            return output

        single = convert("single-load", single_load_fixture())
        check_order("single-load", single.read_bytes())

        ordered = convert("ordered-loads", multi_load_fixture([TEXT, RODATA, PROCPARAM, DATA]))
        headers = check_order("ordered-loads", ordered.read_bytes())
        assert (PT_OS_PROCPARAM, PROCPARAM[3]) in [(h[0], h[3]) for h in headers], headers
        assert [h[3] for h in headers if h[0] == PT_LOAD][:3] == [TEXT[3], RODATA[3], DATA[3]], headers
        run_expecting_trap("ordered-loads", ordered)

        unordered = convert("unordered-loads", multi_load_fixture([DATA, PROCPARAM, TEXT, RODATA]))
        headers = check_order("unordered-loads", unordered.read_bytes())
        assert [h[3] for h in headers if h[0] == PT_LOAD][:3] == [TEXT[3], RODATA[3], DATA[3]], headers
        assert kinds_between_loads(headers) == [PT_OS_PROCPARAM], headers
        run_expecting_trap("unordered-loads", unordered)
    print("Linux program header order test passed")


def kinds_between_loads(headers):
    kinds = [header[0] for header in headers]
    first = kinds.index(PT_LOAD)
    last = len(kinds) - 1 - kinds[::-1].index(PT_LOAD)
    return [kind for kind in kinds[first:last + 1] if kind != PT_LOAD]


if __name__ == "__main__":
    main()
