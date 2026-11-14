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
            "#define _GNU_SOURCE\n#include <dlfcn.h>\n"
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
        for options in ([], ["--linux-fixed-base", "0x400000"]):
            output = work / "output.elf"
            result = subprocess.run([str(relinker), "--skip-sce-module", *options, str(source), str(output)], capture_output=True, text=True)
            assert result.returncode == 0, (options, result.stdout, result.stderr)
            output.chmod(0o755)
            executed = subprocess.run([str(output)], capture_output=True)
            assert executed.returncode == 42, (options, executed.returncode, executed.stderr)
        struct.pack_into("<Q", image, 0x4800 + 7 * 16 + 8, 47)
        source.write_bytes(image)
        invalid = subprocess.run([str(relinker), "--skip-sce-module", str(source), str(work / "invalid.elf")], capture_output=True, text=True)
        assert invalid.returncode == 2 and "dynamic symbol table size" in invalid.stderr, invalid.stderr
        multi_probe = work / "multi_probe.c"
        multi_probe.write_text(
            "#define _GNU_SOURCE\n#include <dlfcn.h>\n"
            "#include <unistd.h>\n"
            "void check(void) {\n"
            "    int (*fa)(void) = dlsym(RTLD_DEFAULT, \"Ae\");\n"
            "    int (*fb)(void) = dlsym(RTLD_DEFAULT, \"Ad\");\n"
            "    int (*fc)(void) = dlsym(RTLD_DEFAULT, \"Ac\");\n"
            "    int (*fd)(void) = dlsym(RTLD_DEFAULT, \"Ab\");\n"
            "    _exit(fa && fb && fc && fd && fa() == 42 && fb() == 42 && fc() == 42 && fd() == 42 ? 42 : 1);\n"
            "}\n")
        multi_lib = work / "multi_probe.so"
        subprocess.run([os.environ.get("CC", "cc"), "-shared", "-fPIC", str(multi_probe), "-o", str(multi_lib), "-ldl", "-Wl,-init,check"], check=True)
        multi_image = fixture()
        multi_names = ["Ae", "Ad", "Ac", "Ab"]
        multi_symbols = [(f"{n}#A#B", 0x12, 0, 1, 0x210, 6) for n in multi_names]
        multi_strings = bytearray(b"\0")
        for index, (name, info, visibility, section, value, size) in enumerate(multi_symbols, 1):
            offset = len(multi_strings)
            multi_strings.extend(name.encode() + b"\0")
            struct.pack_into("<IBBHQQ", multi_image, 0x4900 + index * 24, offset, info, visibility, section, value, size)
        multi_strings = multi_strings.ljust(64, b"\0") + str(multi_lib).encode() + b"\0"
        multi_image[0x4600:0x4600 + len(multi_strings)] = multi_strings
        multi_image[0x4210:0x4216] = bytes.fromhex("b82a000000c3")
        multi_tags = [(5, 0x600), (10, len(multi_strings)), (6, 0x900), (11, 24), (7, 0x700),
                      (8, 0), (9, 24), (0x6100003f, (len(multi_symbols) + 1) * 24), (1, 64), (0, 0)]
        struct.pack_into("<IIQQQQQQ", multi_image, 120, 2, 6, 0x4800, 0x800, 0x800, len(multi_tags) * 16, len(multi_tags) * 16, 8)
        for index, tag in enumerate(multi_tags):
            struct.pack_into("<qQ", multi_image, 0x4800 + index * 16, *tag)
        multi_source = work / "multi_input.elf"
        multi_source.write_bytes(multi_image)
        multi_output = work / "multi_output.elf"
        result = subprocess.run([str(relinker), "--skip-sce-module", str(multi_source), str(multi_output)], capture_output=True, text=True)
        assert result.returncode == 0, (result.stdout, result.stderr)
        multi_output.chmod(0o755)
        executed = subprocess.run([str(multi_output)], capture_output=True)
        assert executed.returncode == 42, (executed.returncode, executed.stderr)
        multi_sysv = work / "multi_sysv.elf"
        sysv_data = bytearray(multi_output.read_bytes())
        found_gnu_hash = False
        for i in range(0, len(sysv_data) - 16, 8):
            if struct.unpack_from("<q", sysv_data, i)[0] == 0x6ffffef5:
                struct.pack_into("<q", sysv_data, i, 0x6ffffdf8)
                found_gnu_hash = True
                break
        assert found_gnu_hash
        multi_sysv.write_bytes(sysv_data)
        multi_sysv.chmod(0o755)
        executed_sysv = subprocess.run([str(multi_sysv)], capture_output=True)
        assert executed_sysv.returncode == 42, (executed_sysv.returncode, executed_sysv.stderr)
    print("Linux executable exports test passed")


if __name__ == "__main__":
    main()
