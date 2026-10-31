from pathlib import Path
import os
import struct
import subprocess
import sys
import tempfile

from guest_import_fixtures import executable
from test_guest_intel_trampolines import main_fixture
from test_guest_tls import guest_with_tls, static_tls_getter
from test_optional_plt import fixture


def image(guest, kind, windows=False):
    if guest:
        data = static_tls_getter(8, 16) if windows else guest_with_tls(True)
        load_header, tls_header = 120, 232
        tls_offset, tls_address = 0x980, 0x2380
        alternate_offset = 0x990
        load_offset = 0x600
    else:
        data = fixture()
        struct.pack_into("<H", data, 56, 3)
        load_header, tls_header = 64, 176
        tls_offset, tls_address = 0x800, 0x800
        alternate_offset = 0x900
        load_offset = 0
        struct.pack_into("<IIQQQQQQ", data, tls_header,
                         7, 4, tls_offset, tls_address, tls_address, 8, 16, 8)
    if kind == "large-tbss":
        tls_offset = 0x9F8 if guest else 0xFF8
        tls_address = 0x23F8 if guest else 0xFF8
        struct.pack_into("<QQQ", data, tls_header + 8, tls_offset, tls_address, tls_address)
        struct.pack_into("<Q", data, tls_header + 40, 0x40)
    content = struct.pack("<Q", 42)
    data[tls_offset:tls_offset + 8] = content
    if kind in ("same-bytes", "different-bytes", "partial-zero", "partial-nonzero", "zero-bss", "nonzero-bss"):
        struct.pack_into("<Q", data, tls_header + 8, alternate_offset)
        data[alternate_offset:alternate_offset + 8] = content
    if kind == "different-bytes":
        data[alternate_offset + 7] ^= 0x80
    if kind in ("partial-zero", "partial-nonzero"):
        struct.pack_into("<Q", data, load_header + 32, tls_offset - load_offset + 4)
        data[tls_offset:tls_offset + 4] = b"\x2a\x01\x02\x03"
        data[tls_offset + 4:tls_offset + 8] = b"\xff" * 4
        data[alternate_offset:alternate_offset + 8] = b"\x2a\x01\x02\x03\0\0\0\0"
        if kind == "partial-nonzero":
            data[alternate_offset + 7] = 1
    if kind in ("zero-bss", "nonzero-bss"):
        struct.pack_into("<Q", data, load_header + 32, tls_offset - load_offset)
        data[alternate_offset:alternate_offset + 8] = bytes(8)
        if kind == "nonzero-bss":
            data[alternate_offset] = 42
    if kind == "empty":
        struct.pack_into("<Q", data, tls_header + 32, 0)
    return data


def main():
    relinker = Path(sys.argv[1]).resolve()
    cases = ("mapped", "same-bytes", "different-bytes", "partial-zero",
             "partial-nonzero", "zero-bss", "nonzero-bss", "empty", "large-tbss")
    invalid = {"different-bytes", "partial-nonzero", "nonzero-bss"}
    with tempfile.TemporaryDirectory(prefix="anyps5-tls-image-") as directory:
        work = Path(directory)
        for guest in (False, True):
            for target, options in (("linux", ()), ("windows", ("--windows",)), ("macos", ("--macos",))):
                for kind in cases:
                    case = work / f"{guest}-{target}-{kind}"
                    case.mkdir()
                    source = case / "input.elf"
                    output = case / "output"
                    if guest:
                        modules = case / "sce_module"
                        modules.mkdir()
                        (modules / "tls.prx").write_bytes(image(True, kind, target == "windows"))
                        source.write_bytes(executable("tls.prx") if target == "windows" else main_fixture())
                    else:
                        source.write_bytes(image(False, kind))
                    result = subprocess.run([str(relinker), *options, *(() if guest else ("--skip-sce-module",)),
                                             str(source), str(output)], capture_output=True, text=True, timeout=10)
                    if kind in invalid:
                        assert result.returncode == 2 and "PT_TLS initialization image differs from PT_LOAD memory" in result.stderr, (case.name, result.stdout, result.stderr)
                        assert not output.exists(), case.name
                    else:
                        assert result.returncode == 0 and output.is_file(), (case.name, result.stdout, result.stderr)
                        expected = {"mapped": 42, "same-bytes": 42, "large-tbss": 42, "partial-zero": 0x0302012a,
                                    "zero-bss": 0, "empty": 0}
                        if guest and target == "windows" and os.name == "nt" and kind in expected:
                            executed = subprocess.run([str(output)], capture_output=True, timeout=10)
                            assert executed.returncode == expected[kind], (case.name, executed.returncode, executed.stdout, executed.stderr)
    print("54 executable and guest TLS image cases passed")


if __name__ == "__main__":
    main()
