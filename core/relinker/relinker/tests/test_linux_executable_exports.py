import os
from pathlib import Path
import platform
import struct
import subprocess
import sys
import tempfile
from test_linux_load_alignment import fixture


def main():
    if sys.platform != "linux" or platform.machine() != "x86_64":
        return
    relinker = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="anyps5-main-export-") as directory:
        work = Path(directory)
        probe = work / "probe.c"
        probe.write_text(
            "#include <dlfcn.h>\n"
            "#include <unistd.h>\n"
            "void check(void) {\n"
            "    int (*function)(void) = dlsym(RTLD_DEFAULT, \"FixtureExport\");\n"
            "    _exit(function && function() == 42 ? 42 : 1);\n"
            "}\n")
        library = work / "probe.so"
        subprocess.run([os.environ.get("CC", "cc"), "-shared", "-fPIC", str(probe), "-o", str(library), "-ldl", "-Wl,-init,check"], check=True)
        image = fixture()
        strings = b"\0FixtureExport#A#B\0".ljust(32, b"\0") + str(library).encode() + b"\0"
        image[0x4600:0x4600 + len(strings)] = strings
        image[0x4210:0x4216] = bytes.fromhex("b82a000000c3")
        struct.pack_into("<IBBHQQ", image, 0x4900, 0, 0, 0, 0, 0, 0)
        struct.pack_into("<IBBHQQ", image, 0x4918, 1, 0x12, 0, 1, 0x210, 6)
        tags = [(5, 0x600), (10, len(strings)), (6, 0x900), (11, 24), (7, 0x700), (8, 0), (9, 24), (0x6100003f, 48), (1, 32), (0, 0)]
        struct.pack_into("<IIQQQQQQ", image, 120, 2, 6, 0x4800, 0x800, 0x800, len(tags) * 16, len(tags) * 16, 8)
        for index, tag in enumerate(tags):
            struct.pack_into("<qQ", image, 0x4800 + index * 16, *tag)
        source = work / "input.elf"
        source.write_bytes(image)
        output = work / "output.elf"
        result = subprocess.run([str(relinker), "--skip-sce-module", str(source), str(output)], capture_output=True, text=True)
        assert result.returncode == 0, (result.stdout, result.stderr)
        output.chmod(0o755)
        executed = subprocess.run([str(output)], capture_output=True)
        assert executed.returncode == 42, (executed.returncode, executed.stderr)
        struct.pack_into("<Q", image, 0x4800 + 7 * 16 + 8, 47)
        source.write_bytes(image)
        invalid = subprocess.run([str(relinker), "--skip-sce-module", str(source), str(work / "invalid.elf")], capture_output=True, text=True)
        assert invalid.returncode == 2 and "dynamic symbol table size" in invalid.stderr, invalid.stderr
    print("Linux executable exports test passed")


if __name__ == "__main__":
    main()
