from pathlib import Path
import os
import json
import struct
import subprocess
import sys
import tempfile


def fixture(second_code: bytes, with_import: bool = False) -> bytearray:
    image = bytearray(0x1000)
    image[:16] = b"\x7fELF\x02\x01\x01" + bytes(9)
    struct.pack_into("<HHIQQQIHHHHHH", image, 16,
                     3, 62, 1, 0x1210, 64, 0, 0, 64, 56, 4, 64, 0, 0)
    image[0x200:0x206] = b"\xff\x25\xfa\xf0\xff\xff"
    image[0x210:0x215] = b"\xe9\xfb\x05\x00\x00"
    image[0x800:0x800 + len(second_code)] = second_code
    struct.pack_into("<QQq", image, 0x700, 0x310, 8, 0x1210)
    tags = ([(1, 4), (5, 0x600), (10, 12), (6, 0x620), (11, 24)] if with_import else
            [(5, 0x600), (10, 1), (6, 0x620), (11, 24)])
    tags += [
            (7, 0x700), (8, 48 if with_import else 24), (9, 24), (0, 0)]
    if with_import:
        tags.insert(-1, (0x6100003f, 48))
        image[0x600:0x600 + len(b"foo\x00lib.so\x00")] = b"foo\x00lib.so\x00"
        struct.pack_into("<QQq", image, 0x718, 0x300, (1 << 32) | 6, 0)
    struct.pack_into("<IIQQQQQQ", image, 64,
                     1, 6, 0, 0, 0, 0x800, 0x800, 0x1000)
    struct.pack_into("<IIQQQQQQ", image, 120,
                     2, 6, 0x400, 0x400, 0x400, len(tags) * 16, len(tags) * 16, 8)
    struct.pack_into("<IIQQQQQQ", image, 176,
                     1, 5, 0x200, 0x1200, 0x1200, 0x20, 0x20, 0x1000)
    struct.pack_into("<IIQQQQQQ", image, 232,
                     1, 5, 0x800, 0x1800, 0x1800, len(second_code), len(second_code), 0x1000)
    for index, tag in enumerate(tags):
        struct.pack_into("<qQ", image, 0x400 + index * 16, *tag)
    return image


def run_case(relinker: Path, work: Path, name: str, args, code: bytes, with_import=False):
    source = work / (name + ".elf")
    output = work / (name + ".exe")
    source.write_bytes(fixture(code, with_import))
    result = subprocess.run([str(relinker), "--skip-sce-module", "--windows", *args,
                             str(source), str(output)], capture_output=True,
                            text=True, timeout=20)
    return result, output


def main():
    relinker = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="anyps5-executable-segments-") as directory:
        work = Path(directory)
        result, output = run_case(relinker, work, "syscall", [], b"\x0f\x05\xc3")
        assert result.returncode == 2 and "Forbidden syscall instruction" in result.stderr and not output.exists(), result

        result, output = run_case(relinker, work, "syscall-filter", ["unused-filter=1"], b"\x0f\x05\xc3")
        assert result.returncode == 2 and "Forbidden syscall instruction" in result.stderr and not output.exists(), result

        forbidden = (
            ("rex-syscall", b"\x48\x0f\x05"),
            ("operand-syscall", b"\x66\x0f\x05"),
            ("address-sysenter", b"\x67\x0f\x34"),
            ("segment-sysenter", b"\x2e\x0f\x34"),
            ("rex-sysret", b"\x48\x0f\x07"),
            ("mixed-sysret", b"\x66\x67\x48\x0f\x07"),
            ("operand-int80", b"\x66\xcd\x80"),
            ("segment-int80", b"\x64\xcd\x80"),
        )
        for name, instruction in forbidden:
            result, output = run_case(relinker, work, name, [], b"\x90" + instruction + b"\xc3")
            assert result.returncode == 2 and not output.exists(), (name, result.stdout, result.stderr)
            assert "Forbidden syscall instruction at code offset 0x1801" in result.stderr, (name, result.stderr)

        result, output = run_case(relinker, work, "immediate-decoys", [],
                                  b"\x48\xb8\x0f\x05\xcd\x80\x0f\x34\x0f\x07\xc3")
        assert result.returncode == 0 and output.exists(), (result.stdout, result.stderr)

        result, output = run_case(relinker, work, "skip-prefixed", ["--skip-syscall-check"], b"\x48\x0f\x05\xc3")
        assert result.returncode == 0 and output.exists(), (result.stdout, result.stderr)

        result, output = run_case(relinker, work, "skip", ["--skip-syscall-check"], b"\x0f\x05\xc3")
        assert result.returncode == 0 and output.exists(), (result.stdout, result.stderr)
        second_code = b"\x90" * 16 + b"\xb8\x2a\x00\x00\x00\xc3"
        result, output = run_case(relinker, work, "execute", ["--skip-syscall-check"], second_code)
        assert result.returncode == 0 and output.exists(), (result.stdout, result.stderr)
        if os.name == "nt":
            executed = subprocess.run([str(output)], capture_output=True, timeout=20)
            assert executed.returncode == 42, executed.returncode

        second_code = b"\x48\x8b\x05\xf9\xea\xff\xff" + b"\x90" * 9 + b"\xb8\x2a\x00\x00\x00\xc3"
        result, output = run_case(relinker, work, "preserve", ["unused-filter=1", "--registry"], second_code, True)
        registry = output.with_name(output.stem + ".registry.json")
        assert result.returncode == 0 and output.exists() and registry.exists(), (result.stdout, result.stderr)
        entries = json.loads(registry.read_text())
        assert len(entries) == 1 and entries[0]["nid"] == "foo", entries
        assert entries[0]["callSites"] == ["0x1200", "0x1800"], entries
        assert "CFG/GOT filtering skipped" in result.stdout and "filtered=0" in result.stdout

        result, output = run_case(relinker, work, "strict", ["unused-filter=2"], b"\x90\xc3")
        assert result.returncode == 2 and "multiple executable segments" in result.stderr and not output.exists(), result

        for name, memory_size in (("overlap-past-end", 0x10000000000000), ("overlap-next", 0x1300)):
            image = fixture(b"\x90\xc3")
            struct.pack_into("<Q", image, 64 + 40, memory_size)
            source = work / (name + ".elf")
            output = work / (name + ".exe")
            source.write_bytes(image)
            result = subprocess.run([str(relinker), "--skip-sce-module", "--windows", str(source), str(output)],
                                    capture_output=True, text=True, timeout=20)
            assert result.returncode == 2 and "Overlapping PT_LOAD memory ranges" in result.stderr and not output.exists(), (name, result)
    print("Executable segment tests passed")


if __name__ == "__main__":
    main()
