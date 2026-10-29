import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

from test_string_table_bounds import fixture


def make_image(strings, needed_offset=1, symbol_offset=8):
    image = fixture(str_size=len(strings), needed_offset=needed_offset, symbol_offset=symbol_offset, table_bytes=strings)
    struct.pack_into("<H", image, 56, 6)
    struct.pack_into("<IIQQQQQQ", image, 64,
                     1, 7, 0x200, 0x200, 0x200, len(image) - 0x200, len(image) - 0x200, 0x1000)
    for slot in (3, 4, 5):
        struct.pack_into("<IIQQQQQQ", image, 64 + slot * 56,
                         0x6fffff01, 0, 0, 0, 0, 0, 0, 1)
    return image


def main():
    relinker = Path(sys.argv[1]).resolve()
    names = ["symbol", 'quote"slash\\', "caf\u00e9-\u007f"]
    names.extend("symbol" + chr(value) for value in range(1, 32))
    names.extend(["\u0080", "\u009f", "\u07ff", "\u0800", "\ud7ff", "\ue000", "\uffff", "\U00010000", "\U0010ffff"])
    invalid_sequences = [
        b"\x80",
        b"\xbf",
        b"\xc0\x80",
        b"\xc1\xbf",
        b"\xf5\x80\x80\x80",
        b"\xff",
        b"\xc2",
        b"\xe0\xa0",
        b"\xf0\x90\x80",
        b"\xc2\x20",
        b"\xe0\xa0\x20",
        b"\xf0\x90\x80\x20",
        b"\xc0\xaf",
        b"\xe0\x9f\xbf",
        b"\xf0\x8f\xbf\xbf",
        b"\xed\xa0\x80",
        b"\xed\xbf\xbf",
        b"\xf4\x90\x80\x80",
    ]
    with tempfile.TemporaryDirectory(prefix="anyps5-call-registry-") as directory:
        work = Path(directory)
        for index, name in enumerate(names):
            strings = b"\0lib.so\0" + name.encode("utf-8") + b"\0"
            image = make_image(strings)
            source = work / (str(index) + ".elf")
            source.write_bytes(image)
            for options in ([], ["--windows"]):
                output = work / (str(index) + ("-windows.exe" if options else "-linux.out"))
                result = subprocess.run([str(relinker), "--skip-sce-module", "--registry", *options,
                                         str(source), str(output)], capture_output=True, timeout=20)
                if result.returncode != 0 or not output.exists():
                    raise AssertionError((name, options, result.returncode, result.stdout, result.stderr))
                registry = output.with_suffix(".registry.json")
                entries = json.loads(registry.read_text(encoding="utf-8"))
                expected = [{"nid": name, "library": "", "relocationType": "R_X86_64_GLOB_DAT",
                             "relocationOffset": "0x700", "targetSection": ".got", "targetOffset": "0x300",
                             "callSites": ["0x200"], "callSitesResolved": True, "weak": False}]
                if entries != expected:
                    raise AssertionError((name, options, entries))
                needed = json.loads(output.with_suffix(".needed.json").read_text(encoding="utf-8"))
                if needed != ["lib.so"]:
                    raise AssertionError((name, options, needed))
        for index, invalid_seq in enumerate(invalid_sequences):
            strings = b"\0lib.so\0" + invalid_seq + b"\0"
            image = make_image(strings)
            source = work / f"invalid_{index}.elf"
            source.write_bytes(image)
            for options in ([], ["--windows"]):
                output_ok = work / f"invalid_{index}_ok{'-windows.exe' if options else '-linux.out'}"
                result_ok = subprocess.run([str(relinker), "--skip-sce-module", *options,
                                            str(source), str(output_ok)], capture_output=True, timeout=20)
                if result_ok.returncode != 0 or not output_ok.exists():
                    raise AssertionError((invalid_seq, options, result_ok.returncode, result_ok.stdout, result_ok.stderr))
                output_bad = work / f"invalid_{index}_bad{'-windows.exe' if options else '-linux.out'}"
                result_bad = subprocess.run([str(relinker), "--skip-sce-module", "--registry", *options,
                                             str(source), str(output_bad)], capture_output=True, timeout=20)
                registry = output_bad.with_suffix(".registry.json")
                if result_bad.returncode != 2 or b"UTF-8" not in result_bad.stderr or registry.exists():
                    raise AssertionError((invalid_seq, options, result_bad.returncode, result_bad.stdout, result_bad.stderr))
        needed_strings = b"\0\xff\0symbol\0"
        needed_image = make_image(needed_strings, needed_offset=1, symbol_offset=3)
        needed_source = work / "needed_invalid.elf"
        needed_source.write_bytes(needed_image)
        output_needed_ok = work / "needed_invalid_ok-linux.out"
        result_needed_ok = subprocess.run([str(relinker), "--skip-sce-module",
                                           str(needed_source), str(output_needed_ok)], capture_output=True, timeout=20)
        if result_needed_ok.returncode != 0 or not output_needed_ok.exists():
            raise AssertionError((result_needed_ok.returncode, result_needed_ok.stdout, result_needed_ok.stderr))
        output_needed_bad = work / "needed_invalid_bad-linux.out"
        result_needed_bad = subprocess.run([str(relinker), "--skip-sce-module", "--registry",
                                            str(needed_source), str(output_needed_bad)], capture_output=True, timeout=20)
        needed_json = output_needed_bad.with_suffix(".needed.json")
        if result_needed_bad.returncode != 2 or b"UTF-8" not in result_needed_bad.stderr or needed_json.exists():
            raise AssertionError((result_needed_bad.returncode, result_needed_bad.stdout, result_needed_bad.stderr))
        for relocation_type, type_name in ((1, "R_X86_64_64"), (2, "R_X86_64_PC32"),
                                           (6, "R_X86_64_GLOB_DAT"), (10, "R_X86_64_32"),
                                           (16, "R_X86_64_DTPMOD64"), (17, "R_X86_64_DTPOFF64"),
                                           (18, "R_X86_64_TPOFF64")):
            image = make_image(b"\0lib.so\0symbol\0")
            struct.pack_into("<Q", image, 0x708, (1 << 32) | relocation_type)
            source = work / ("type-" + str(relocation_type) + ".elf")
            source.write_bytes(image)
            output = work / ("type-" + str(relocation_type) + "-linux.out")
            result = subprocess.run([str(relinker), "--skip-sce-module", "--registry",
                                     str(source), str(output)], capture_output=True, timeout=20)
            if result.returncode != 0 or not output.exists():
                raise AssertionError((relocation_type, result.returncode, result.stdout, result.stderr))
            entries = json.loads(output.with_suffix(".registry.json").read_text(encoding="utf-8"))
            if len(entries) != 1 or entries[0]["relocationType"] != type_name:
                raise AssertionError((relocation_type, type_name, entries))
        strings = b"\0lib.so\0" + names[0].encode("utf-8") + b"\0"
        image = make_image(strings)
        image[0x63C] = 0x20
        source = work / "weak.elf"
        source.write_bytes(image)
        output = work / "weak-windows.exe"
        result = subprocess.run([str(relinker), "--skip-sce-module", "--registry", "--windows",
                                 str(source), str(output)], capture_output=True, timeout=20)
        if result.returncode != 0 or not output.exists():
            raise AssertionError((result.returncode, result.stdout, result.stderr))
        entries = json.loads(output.with_suffix(".registry.json").read_text(encoding="utf-8"))
        if not entries or entries[0]["weak"] is not True:
            raise AssertionError(("weak", entries))
    print("Call registry JSON tests passed")


if __name__ == "__main__":
    main()
