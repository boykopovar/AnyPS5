from pathlib import Path
import os
import struct
import subprocess
import sys
import tempfile

from test_guest_module_args import args_module


def lifecycle_module():
    image = args_module()
    tags = []
    for offset in range(0x2000, 0x2300, 16):
        tag, value = struct.unpack_from("<qQ", image, offset)
        if tag == 0:
            break
        tags.append((tag, 72 if tag == 8 else value))
    tags += [(13, 0x1080), (25, 0x2880), (27, 8), (26, 0x2888), (28, 8), (0, 0)]
    for index, tag in enumerate(tags):
        struct.pack_into("<qQ", image, 0x2000 + index * 16, *tag)
    struct.pack_into("<QQ", image, 120 + 32, len(tags) * 16, len(tags) * 16)
    struct.pack_into("<QQq", image, 0x2718, 0x2880, 8, 0x10c0)
    struct.pack_into("<QQq", image, 0x2730, 0x2888, 8, 0x1100)
    for address, marker in ((0x1080, 0xfe), (0x10c0, 0xa1), (0x1100, 0xf1)):
        code = b"\x48\x83\xec\x08\xbf" + struct.pack("<I", marker) + b"\x31\xf6\xff\x15"
        code += struct.pack("<i", 0x2820 - address - len(code) - 4)
        code += b"\x48\x83\xc4\x08\xc3"
        image[address:address + len(code)] = code
    return image


def main():
    relinker, executable, libraries = map(lambda p: Path(p).resolve(), sys.argv[1:4])
    with tempfile.TemporaryDirectory(prefix="anyps5-runtime-lifecycle-") as directory:
        root = Path(directory)
        plugins = root / "Media" / "Plugins"
        plugins.mkdir(parents=True)
        (plugins / "args.prx").write_bytes(lifecycle_module())
        from test_guest_module_directories import module_with_symbol
        bundled = root / "sce_module"
        bundled.mkdir()
        (bundled / "dummy.prx").write_bytes(module_with_symbol(False))
        source = root / "main.elf"
        from test_guest_intel_trampolines import main_fixture
        source.write_bytes(main_fixture())
        run = subprocess.run([str(relinker), "--skip-syscall-check", "--module-dir", "Media/Plugins", "--rpath", str(libraries), str(source), str(root / "app.elf")], capture_output=True, text=True, timeout=30)
        assert run.returncode == 0, (run.stdout, run.stderr)
        module = root / "app0/Media/Plugins/args.prx.guest.prx"
        for mode in ("never-start", "start", "zero-start", "concurrent", "reentrant", "reload"):
            run = subprocess.run([str(executable), "./" + module.name, mode], cwd=module.parent, env=os.environ, capture_output=True, text=True, timeout=10)
            assert run.returncode == 0, (mode, run.returncode, run.stdout, run.stderr)
    print("Runtime module initialization and finalization passed")


if __name__ == "__main__":
    main()
