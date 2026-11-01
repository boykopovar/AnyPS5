import struct
import subprocess
import sys
import tempfile
from pathlib import Path

CODE = 0x1000
GOT_PUTS = 0x2090
GOT_STRLEN = 0x2098
MESSAGE = 0x2500
EXIT_TAIL = b"\xbf\x2a\x00\x00\x00\xb8\xe7\x00\x00\x00\x0f\x05"


def build(n_nops, tail=EXIT_TAIL):
    image = bytearray(0x3000)
    image[:16] = b"\x7fELF\x02\x01\x01" + bytes(9)
    nph = 6
    struct.pack_into("<HHIQQQIHHHHHH", image, 16, 3, 62, 1, CODE, 64, 0, 0, 64, 56, nph, 0, 0, 0)

    def phdr(index, p_type, flags, offset, vaddr, filesz, memsz, align):
        struct.pack_into("<IIQQQQQQ", image, 64 + index * 56, p_type, flags, offset, vaddr, vaddr, filesz, memsz, align)

    interpreter = b"/lib64/ld-linux-x86-64.so.2\0"
    image[0x200:0x200 + len(interpreter)] = interpreter
    phdr(0, 6, 4, 64, 64, nph * 56, nph * 56, 8)
    phdr(1, 3, 4, 0x200, 0x200, len(interpreter), len(interpreter), 1)
    phdr(5, 0x6474e551, 6, 0, 0, 0, 0, 0x10)
    message = b"hello from AnyPS5\0"
    image[MESSAGE:MESSAGE + len(message)] = message
    lea = b"\x48\x8d\x3d" + struct.pack("<i", MESSAGE - (CODE + 7))
    call = b"\xff\x15" + struct.pack("<i", GOT_PUTS - (CODE + 13))
    code = lea + call + tail + b"\x90" * n_nops
    image[CODE:CODE + len(code)] = code
    phdr(2, 1, 5, 0, 0x0000, 0x2000, 0x2000, 0x1000)
    phdr(3, 1, 6, 0x2000, 0x2000, 0x1000, 0x1000, 0x1000)
    struct.pack_into("<Q", image, GOT_PUTS, 0)
    struct.pack_into("<Q", image, GOT_STRLEN, 0)
    strtab = b"\0libc.so.6\0puts\0strlen\0"
    image[0x2100:0x2100 + len(strtab)] = strtab
    struct.pack_into("<IBBHQQ", image, 0x2120, 0, 0, 0, 0, 0, 0)
    struct.pack_into("<IBBHQQ", image, 0x2138, 11, 0x12, 0, 0, 0, 0)
    struct.pack_into("<IBBHQQ", image, 0x2150, 16, 0x12, 0, 0, 0, 0)
    struct.pack_into("<IIIII", image, 0x2168, 1, 3, 1, 2, 0)
    struct.pack_into("<QQq", image, 0x2194, GOT_PUTS, (1 << 32) | 6, 0)
    struct.pack_into("<QQq", image, 0x21ac, GOT_STRLEN, (2 << 32) | 6, 0)
    tags = [(1, 1), (5, 0x2100), (10, len(strtab)), (6, 0x2120), (11, 24),
            (4, 0x2168), (7, 0x2194), (8, 48), (9, 24), (0, 0)]
    for index, tag in enumerate(tags):
        struct.pack_into("<qQ", image, 0x2200 + index * 16, *tag)
    phdr(4, 2, 6, 0x2200, 0x2200, len(tags) * 16, len(tags) * 16, 8)
    return bytes(image)


def run(relinker, source, output):
    return subprocess.run(
        [str(relinker), "--skip-sce-module", "--skip-syscall-check", "unused-filter=1",
         str(source), str(output)],
        capture_output=True, text=True, timeout=20)


def main():
    relinker = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="anyps5-unused-filter-fallthrough-") as directory:
        work = Path(directory)
        for n_nops in (0, 1, 2, 3, 4):
            source = work / f"tail-{n_nops}.elf"
            output = work / f"tail-{n_nops}.out"
            source.write_bytes(build(n_nops))
            result = run(relinker, source, output)
            assert result.returncode == 0 and output.exists(), (n_nops, result.returncode, result.stdout, result.stderr)
            assert "CFG/GOT filtering: 2 -> 1; filtered=1" in result.stdout, (n_nops, result.stdout)

        entry_tail = bytearray(build(0))
        struct.pack_into("<Q", entry_tail, 24, 0x1fff)
        source = work / "entry-tail.elf"
        output = work / "entry-tail.out"
        source.write_bytes(entry_tail)
        result = run(relinker, source, output)
        assert result.returncode == 2 and not output.exists(), (result.returncode, result.stdout, result.stderr)
        assert "Instruction truncated before ModRM byte" in result.stderr, result.stderr

        jump_tail = bytearray(build(0))
        jump_tail[CODE:CODE + 5] = b"\xe9" + struct.pack("<i", 0x1fff - (CODE + 5))
        source = work / "jump-tail.elf"
        output = work / "jump-tail.out"
        source.write_bytes(jump_tail)
        result = run(relinker, source, output)
        assert result.returncode == 2 and not output.exists(), (result.returncode, result.stdout, result.stderr)
        assert "Instruction truncated before ModRM byte" in result.stderr, result.stderr

        source = work / "zero-immediate.elf"
        output = work / "zero-immediate.out"
        source.write_bytes(build(0, tail=b"\xb8\x00\x00\x00\x00"))
        result = run(relinker, source, output)
        assert result.returncode == 0 and output.exists(), (result.returncode, result.stdout, result.stderr)
        assert "CFG/GOT filtering: 2 -> 1; filtered=1" in result.stdout, result.stdout
    print("Unused filter fallthrough padding tests passed")


if __name__ == "__main__":
    main()
