"""Check Linux output preserves GNU stack and RELRO program headers."""

from pathlib import Path
import signal
import struct
import subprocess
import sys
import tempfile

from signal_run import run_without_core_dump
from test_linux_load_alignment import fixture

PT_GNU_STACK = 0x6474E551
PT_GNU_RELRO = 0x6474E552
PT_SCE_VERSION = 0x6FFFFF01
DEFAULT_BASE = 0x400000
BASE = 0x800000


def image():
    elf = fixture()
    struct.pack_into("<H", elf, 56, 10)
    struct.pack_into("<I", elf, 68, 7)
    for index in range(2, 10):
        struct.pack_into("<IIQQQQQQ", elf, 64 + index * 56, PT_SCE_VERSION, 0, 0, 0, 0, 0, 0, 1)
    struct.pack_into("<IIQQQQQQ", elf, 64 + 2 * 56, PT_GNU_STACK, 6, 0, 0, 0, 0, 0, 16)
    struct.pack_into("<IIQQQQQQ", elf, 64 + 3 * 56, PT_GNU_RELRO, 4, 0x4500, 0x500, 0x500, 0x100, 0x100, 1)
    return elf


def headers(elf):
    offset, = struct.unpack_from("<Q", elf, 0x20)
    entry_size, count = struct.unpack_from("<HH", elf, 0x36)
    return [struct.unpack_from("<IIQQQQQQ", elf, offset + index * entry_size) for index in range(count)]


def executable_image(relro_memory_size):
    elf = image()
    struct.pack_into("<Q", elf, 24, 0x10)
    elf[0x4010] = 0xcc
    struct.pack_into("<IIQQQQQQ", elf, 64 + 4 * 56, 1, 6, 0x5000, 0x4000, 0x4000, 0x1000, 0x4000, 0x1000)
    struct.pack_into("<IIQQQQQQ", elf, 64 + 3 * 56, PT_GNU_RELRO, 4, 0x5000, 0x4000, 0x4000, 0x1000, relro_memory_size, 1)
    return elf


def main():
    relinker = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="anyps5-linux-gnu-segments-") as directory:
        work = Path(directory)
        source = work / "input.elf"
        source.write_bytes(image())
        for name, options, base, keeps_relro in (("default", [], DEFAULT_BASE, True), ("fixed", ["--linux-fixed-base", hex(BASE)], BASE, True), ("lazy", ["--lazy-binding"], 0, False)):
            output = work / (name + ".elf")
            result = subprocess.run([str(relinker), "--skip-sce-module", *options, str(source), str(output)],
                                    capture_output=True, text=True, timeout=20)
            assert result.returncode == 0, (result.stdout, result.stderr)
            found = {header[0]: header for header in headers(output.read_bytes())}
            assert PT_GNU_STACK in found, "PT_GNU_STACK was dropped"
            assert (PT_GNU_RELRO in found) == keeps_relro, "PT_GNU_RELRO presence does not match the binding mode"
            stack = found[PT_GNU_STACK]
            assert stack[1] == 6 and stack[2:6] == (0, 0, 0, 0), stack
            if keeps_relro:
                relro = found[PT_GNU_RELRO]
                assert relro[3:5] == (0x500 + base, 0x500 + base), relro
                assert relro[5:7] == (0x100, 0x100), relro

        dropped_stack = image()
        struct.pack_into("<IIQQQQQQ", dropped_stack, 64 + 2 * 56, PT_SCE_VERSION, 0, 0, 0, 0, 0, 0, 1)
        source.write_bytes(dropped_stack)
        output = work / "synthesized.elf"
        result = subprocess.run([str(relinker), "--skip-sce-module", str(source), str(output)],
                                capture_output=True, text=True, timeout=20)
        assert result.returncode == 0, (result.stdout, result.stderr)
        found = {header[0]: header for header in headers(output.read_bytes())}
        assert found[PT_GNU_STACK] == (PT_GNU_STACK, 6, 0, 0, 0, 0, 0, 16), found
        assert found[PT_GNU_RELRO][3:5] == (0x500 + DEFAULT_BASE, 0x500 + DEFAULT_BASE), found

        if sys.platform.startswith("linux"):
            for options, mode in (([], "default"), (["--linux-fixed-base", hex(BASE)], "fixed")):
                for relro_memory_size, keeps_relro in ((0x4000, True), (0x8000, False)):
                    source.write_bytes(executable_image(relro_memory_size))
                    output = work / f"execute-{mode}-{relro_memory_size:x}.elf"
                    result = subprocess.run([str(relinker), "--skip-sce-module", *options, str(source), str(output)],
                                            capture_output=True, text=True, timeout=20)
                    assert result.returncode == 0, (result.stdout, result.stderr)
                    found = {header[0]: header for header in headers(output.read_bytes())}
                    assert (PT_GNU_RELRO in found) == keeps_relro, "PT_GNU_RELRO was kept without writable load coverage"
                    output.chmod(output.stat().st_mode | 0o111)
                    execution = run_without_core_dump([str(output)], capture_output=True, text=True, timeout=20)
                    assert execution.returncode == -signal.SIGTRAP, (execution.returncode, execution.stdout, execution.stderr)

            synthesized_executable = executable_image(0x4000)
            struct.pack_into("<IIQQQQQQ", synthesized_executable, 64 + 2 * 56, PT_SCE_VERSION, 0, 0, 0, 0, 0, 0, 1)
            source.write_bytes(synthesized_executable)
            output = work / "execute-synthesized-stack.elf"
            result = subprocess.run([str(relinker), "--skip-sce-module", str(source), str(output)],
                                    capture_output=True, text=True, timeout=20)
            assert result.returncode == 0, (result.stdout, result.stderr)
            found = {header[0]: header for header in headers(output.read_bytes())}
            assert found[PT_GNU_STACK] == (PT_GNU_STACK, 6, 0, 0, 0, 0, 0, 16), found
            assert found[PT_GNU_RELRO][3:5] == (0x4000 + DEFAULT_BASE, 0x4000 + DEFAULT_BASE), found
            output.chmod(output.stat().st_mode | 0o111)
            execution = run_without_core_dump([str(output)], capture_output=True, text=True, timeout=20)
            assert execution.returncode == -signal.SIGTRAP, (execution.returncode, execution.stdout, execution.stderr)
    print("Linux GNU segment tests passed")


if __name__ == "__main__":
    main()
