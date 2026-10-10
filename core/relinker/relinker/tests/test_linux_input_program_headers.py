"""Check that a Linux relink replaces the input's PT_PHDR and PT_INTERP with its own instead of copying them."""

from pathlib import Path
import platform
import signal
import struct
import subprocess
import sys
import tempfile

PT_LOAD = 1
PT_DYNAMIC = 2
PT_INTERP = 3
PT_PHDR = 6
PT_SCE_VERSION = 0x6FFFFF01
HOST_INTERP = b"/lib64/ld-linux-x86-64.so.2\0"
INPUT_INTERP = b"/libexec/ld-elf.so.1\0"
TAGS = [(5, 0x2600), (10, 1), (6, 0x2620), (11, 24), (7, 0x2700), (8, 0), (9, 24), (0, 0)]


def fixture(phdr, interp, slots):
    image = bytearray(0x3000)
    image[:16] = b"\x7fELF\x02\x01\x01" + bytes(9)
    headers = []
    if phdr:
        headers.append((PT_PHDR, 4, 0x40, 0x40, slots * 56, slots * 56, 8))
    if interp:
        headers.append((PT_INTERP, 4, 0x300, 0x300, len(INPUT_INTERP), len(INPUT_INTERP), 1))
    headers += [(PT_LOAD, 4, 0x0000, 0x0000, 0x1000, 0x1000, 0x1000),
                (PT_LOAD, 5, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000),
                (PT_LOAD, 6, 0x2000, 0x2000, 0x1000, 0x1000, 0x1000),
                (PT_DYNAMIC, 6, 0x2400, 0x2400, len(TAGS) * 16, len(TAGS) * 16, 8)]
    assert len(headers) <= slots, (len(headers), slots)
    headers += [(PT_SCE_VERSION, 0, 0, 0, 0, 0, 1)] * (slots - len(headers))
    struct.pack_into("<HHIQQQIHHHHHH", image, 16, 3, 62, 1, 0x1000, 64, 0, 0, 64, 56, slots, 64, 0, 0)
    for index, (kind, flags, offset, vaddr, filesz, memsz, align) in enumerate(headers):
        struct.pack_into("<IIQQQQQQ", image, 64 + index * 56, kind, flags, offset, vaddr, vaddr, filesz, memsz, align)
    image[0x300:0x300 + len(INPUT_INTERP)] = INPUT_INTERP
    image[0x1000:0x1001] = b"\xcc"
    for index, tag in enumerate(TAGS):
        struct.pack_into("<qQ", image, 0x2400 + index * 16, *tag)
    return image


def program_headers(elf):
    phoff, = struct.unpack_from("<Q", elf, 0x20)
    phentsize, phnum = struct.unpack_from("<HH", elf, 0x36)
    return phoff, phnum, [struct.unpack_from("<IIQQQQQQ", elf, phoff + index * phentsize) for index in range(phnum)]


def check(name, elf):
    phoff, phnum, headers = program_headers(elf)
    phdrs = [header for header in headers if header[0] == PT_PHDR]
    interps = [header for header in headers if header[0] == PT_INTERP]
    loads = [header for header in headers if header[0] == PT_LOAD]
    assert len(phdrs) == 1 and len(interps) == 1, (name, [header[0] for header in headers])
    phdr = phdrs[0]
    assert headers[0] == phdr and phdr[2] == phoff and phdr[5] == phnum * 56, (name, phdr, phoff, phnum)
    covering = [load for load in loads if load[2] <= phoff and phoff + phnum * 56 <= load[2] + load[5]
                and load[3] - load[2] == phdr[3] - phdr[2]]
    assert covering, ("PT_PHDR address must match the PT_LOAD that maps the table", name, phdr, loads)
    interp = interps[0]
    assert elf[interp[2]:interp[2] + interp[5]] == HOST_INTERP, (name, elf[interp[2]:interp[2] + interp[5]])
    assert headers.index(interp) < headers.index(loads[0]), (name, [header[0] for header in headers])
    return headers


def run_expecting_trap(name, output):
    if not (sys.platform.startswith("linux") and platform.machine() in ("x86_64", "AMD64")):
        return
    output.chmod(0o755)
    executed = subprocess.run([str(output)], capture_output=True, timeout=20)
    assert executed.returncode == -signal.SIGTRAP, (name, executed.returncode, executed.stderr)


def main():
    relinker = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="anyps5-input-phdrs-") as directory:
        work = Path(directory)

        def convert(name, image):
            source = work / (name + ".elf")
            output = work / (name + ".out.elf")
            source.write_bytes(image)
            result = subprocess.run([str(relinker), "--skip-sce-module", str(source), str(output)],
                                    capture_output=True, text=True, timeout=20)
            assert result.returncode == 0, (name, result.stdout, result.stderr)
            elf = output.read_bytes()
            check(name, elf)
            run_expecting_trap(name, output)
            return elf

        convert("neither", fixture(False, False, 9))
        convert("interp-only", fixture(False, True, 9))
        convert("phdr-only", fixture(True, False, 9))
        both = convert("both", fixture(True, True, 9))
        exact = convert("both-exact-slots", fixture(True, True, 8))
        assert program_headers(exact)[1] == 8, program_headers(exact)[1]
        assert program_headers(both)[1] == 8, program_headers(both)[1]
    print("Linux input program headers test passed")


if __name__ == "__main__":
    main()
