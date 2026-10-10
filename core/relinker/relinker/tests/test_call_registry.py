import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

from test_string_table_bounds import fixture


def main():
    relinker = Path(sys.argv[1]).resolve()
    names = ["symbol", 'quote"slash\\', "caf\u00e9-\u007f"]
    names.extend("symbol" + chr(value) for value in range(1, 32))
    with tempfile.TemporaryDirectory(prefix="anyps5-call-registry-") as directory:
        work = Path(directory)
        for index, name in enumerate(names):
            strings = b"\0lib.so\0" + name.encode("utf-8") + b"\0"
            image = fixture(str_size=len(strings), table_bytes=strings)
            struct.pack_into("<H", image, 56, 6)
            struct.pack_into("<IIQQQQQQ", image, 64,
                             1, 7, 0x200, 0x200, 0x200, len(image) - 0x200, len(image) - 0x200, 0x1000)
            for slot in (3, 4, 5):
                struct.pack_into("<IIQQQQQQ", image, 64 + slot * 56,
                                 0x6fffff01, 0, 0, 0, 0, 0, 0, 1)
            source = work / (str(index) + ".elf")
            source.write_bytes(image)
            for options in ([], ["--windows"]):
                output = work / (str(index) + ("-windows.exe" if options else "-linux.out"))
                result = subprocess.run([str(relinker), "--skip-sce-module", "--registry", *options,
                                         str(source), str(output)], capture_output=True, text=True, timeout=20)
                if result.returncode != 0 or not output.exists():
                    raise AssertionError((name, options, result.returncode, result.stdout, result.stderr))
                registry = output.with_suffix(".registry.json")
                entries = json.loads(registry.read_text(encoding="utf-8"))
                expected = [{"nid": name, "library": "", "relocationType": "R_X86_64_GLOB_DAT",
                             "relocationOffset": "0x700", "targetSection": ".got", "targetOffset": "0x300",
                             "callSites": ["0x200"], "callSitesResolved": True}]
                if entries != expected:
                    raise AssertionError((name, options, entries))
    print("Call registry JSON tests passed")


if __name__ == "__main__":
    main()
