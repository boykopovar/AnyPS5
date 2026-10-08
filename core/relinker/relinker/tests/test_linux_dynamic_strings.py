"""Check that DT_STRSZ and the .dynstr section of a Linux relink cover the DT_RUNPATH string."""

from pathlib import Path
import struct
import subprocess
import sys
import tempfile

from test_linux_load_alignment import fixture

PT_LOAD = 1
PT_DYNAMIC = 2
DT_NULL = 0
DT_STRTAB = 5
DT_STRSZ = 10
DT_RUNPATH = 29


def file_offset(elf, segments, vaddr):
    for segment in segments:
        if segment[0] == PT_LOAD and segment[3] <= vaddr < segment[3] + segment[5]:
            return segment[2] + vaddr - segment[3]
    raise AssertionError(("address is not file-backed", hex(vaddr)))


def dynamic_strings(elf):
    phoff, = struct.unpack_from("<Q", elf, 0x20)
    phentsize, phnum = struct.unpack_from("<HH", elf, 0x36)
    segments = [struct.unpack_from("<IIQQQQQQ", elf, phoff + index * phentsize) for index in range(phnum)]
    dynamic = next(segment for segment in segments if segment[0] == PT_DYNAMIC)
    tags = {}
    for offset in range(dynamic[2], dynamic[2] + dynamic[5], 16):
        tag, value = struct.unpack_from("<qQ", elf, offset)
        if tag == DT_NULL:
            break
        tags[tag] = value
    table = file_offset(elf, segments, tags[DT_STRTAB])
    return tags, elf[table:table + tags[DT_STRSZ]]


def dynstr_section_size(elf):
    shoff, = struct.unpack_from("<Q", elf, 0x28)
    shentsize, shnum, shstrndx = struct.unpack_from("<HHH", elf, 0x3A)
    sections = [struct.unpack_from("<IIQQQQIIQQ", elf, shoff + index * shentsize) for index in range(shnum)]
    names = sections[shstrndx][4]
    for section in sections:
        name = elf[names + section[0]:elf.index(b"\0", names + section[0])]
        if name == b".dynstr":
            return section[5]
    raise AssertionError("missing .dynstr section")


def main():
    relinker = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="anyps5-dynstr-") as directory:
        source = Path(directory) / "input.elf"
        source.write_bytes(fixture())
        for name, options, expected in (("default", [], b"$ORIGIN/libs"),
                                        ("custom", ["--rpath", "/opt/anyps5/lib"], b"/opt/anyps5/lib")):
            output = Path(directory) / (name + ".elf")
            result = subprocess.run([str(relinker), "--skip-sce-module", *options, str(source), str(output)],
                                    capture_output=True, text=True, timeout=20)
            if result.returncode != 0:
                raise AssertionError((name, result.returncode, result.stdout, result.stderr))
            elf = output.read_bytes()
            tags, strings = dynamic_strings(elf)
            run_path = tags[DT_RUNPATH]
            end = strings.find(b"\0", run_path)
            if run_path >= len(strings) or end < 0 or strings[run_path:end] != expected:
                raise AssertionError((name, "DT_RUNPATH is outside DT_STRSZ", run_path, tags[DT_STRSZ]))
            if dynstr_section_size(elf) != tags[DT_STRSZ]:
                raise AssertionError((name, ".dynstr size differs from DT_STRSZ", dynstr_section_size(elf), tags[DT_STRSZ]))
    print("Linux dynamic string tests passed")


if __name__ == "__main__":
    main()
