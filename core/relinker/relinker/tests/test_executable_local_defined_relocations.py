"""An executable relocation against a symbol it defines with STB_LOCAL binding, or with STV_HIDDEN or
STV_INTERNAL visibility, is resolved to that definition as a base-relative relocation. R_X86_64_64
keeps its addend, R_X86_64_GLOB_DAT drops it as the ABI defines, and the Linux, Windows and macOS
outputs apply the link-time address at load time. Other relocation types, absolute definitions and
definitions outside every PT_LOAD still fail the conversion."""

from pathlib import Path
import struct
import sys
import tempfile

from test_executable_tls_relocations import DATA_SYMBOL, dynamic_relocations, image, relink
from test_tls_function_coverage import pe_bytes_at

RELATIVE, R64, GLOB_DAT, PC32 = 8, 1, 6, 2
LOCAL, HIDDEN, INTERNAL = 0x01, 2, 1
SYMBOL_ENTRY = 0x4900 + DATA_SYMBOL * 24
SYMBOL_BYTES = (DATA_SYMBOL + 1) * 24
DATA_NAME = 5
BASE = 0x400000
PE_BASE = 0x10000


def symbol(data, binding=0x11, visibility=0, section=1, value=0x1100):
    struct.pack_into("<IBBHQQ", data, SYMBOL_ENTRY, DATA_NAME, binding, visibility, section, value, 8)
    return data


def main():
    relinker = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="anyps5-local-defined-") as directory:
        for name, binding, visibility in (("local", LOCAL, 0), ("hidden", 0x11, HIDDEN), ("internal", 0x11, INTERNAL)):
            data = symbol(image([(0x1000, R64, DATA_SYMBOL, 8), (0x1008, GLOB_DAT, DATA_SYMBOL, 0)],
                                symbol_bytes=SYMBOL_BYTES), binding, visibility)
            result, output = relink(relinker, directory, name, data)
            assert result.returncode == 0, (name, result.returncode, result.stdout, result.stderr)
            relocations = [entry for entry in dynamic_relocations(output.read_bytes())
                           if entry[0] in (BASE + 0x1000, BASE + 0x1008)]
            assert relocations == [(BASE + 0x1000, RELATIVE, "", 0, BASE + 0x1108),
                                   (BASE + 0x1008, RELATIVE, "", 0, BASE + 0x1100)], relocations

            result, output = relink(relinker, directory, name + "-windows", data, "--windows")
            assert result.returncode == 0, (name, result.returncode, result.stdout, result.stderr)
            pe = output.read_bytes()
            values = [struct.unpack("<Q", pe_bytes_at(pe, PE_BASE + address, 8))[0] for address in (0x1000, 0x1008)]
            assert values == [BASE + 0x1108, BASE + 0x1100], values

            result, output = relink(relinker, directory, name + "-macos", data, "--macos")
            assert result.returncode == 0, (name, result.returncode, result.stdout, result.stderr)

        for name, relocation, binding, visibility, section, value, message in (
            ("pc32", (0x1000, PC32, DATA_SYMBOL, 0), 0x11, HIDDEN, 1, 0x1100, "unsupported relocation type"),
            ("absolute", (0x1000, R64, DATA_SYMBOL, 0), 0x11, HIDDEN, 0xfff1, 0x1100, "unsupported section"),
            ("unmapped", (0x1000, R64, DATA_SYMBOL, 0), 0x11, HIDDEN, 1, 0x90000, "outside a PT_LOAD segment"),
            ("glob-dat-addend", (0x1000, GLOB_DAT, DATA_SYMBOL, 4), 0x11, HIDDEN, 1, 0x1100, "nonzero addend"),
        ):
            data = symbol(image([relocation], symbol_bytes=SYMBOL_BYTES), binding, visibility, section, value)
            result, output = relink(relinker, directory, name, data)
            assert result.returncode == 2 and message in result.stderr, (name, result.returncode, result.stderr)
            assert not output.exists(), name
    print("Executable local defined relocation tests passed")


if __name__ == "__main__":
    main()
