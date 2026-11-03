from pathlib import Path
import os
import shutil
import struct
import subprocess
import sys
import tempfile

from test_guest_intel_trampolines import main_fixture
from test_guest_module_args import args_module, load_and_record
from test_guest_module_directories import module_with_symbol, needed_libraries
from test_guest_needed_modules import executable_with_needed, module_with_needed
from test_guest_symbol_names import DYNAMIC, GOT, RELOCATIONS, importing_executable
from test_macos_output import MachO


def page_aligned(image):
    aligned = bytearray(0x3000)
    aligned[:0x400] = image[:0x400]
    for index in range(3):
        offset = 64 + index * 56
        header = list(struct.unpack_from("<IIQQQQQQ", image, offset))
        aligned[header[3]:header[3] + header[5]] = image[header[2]:header[2] + header[5]]
        header[2] = header[3]
        if header[0] == 1:
            header[7] = 0x1000
        struct.pack_into("<IIQQQQQQ", aligned, offset, *header)
    return aligned


def importing_main(root):
    image = importing_executable(["AAAAAAAAAAA"])
    struct.pack_into("<IIIII", image, 0x4f00, 1, 2, 1, 0, 0)
    for position in range(DYNAMIC, DYNAMIC + 0x200, 16):
        if struct.unpack_from("<q", image, position)[0] == 0:
            struct.pack_into("<qQqQ", image, position, 4, 0x4f00, 0, 0)
            size = position + 32 - DYNAMIC
            break
    if root == "noop":
        struct.pack_into("<Q", image, RELOCATIONS + 8, 1 << 32)
    for position in range(DYNAMIC, DYNAMIC + size, 16):
        tag, value = struct.unpack_from("<qQ", image, position)
        if tag in (4, 5, 6, 7):
            struct.pack_into("<Q", image, position + 8, value + 0x4000)
        elif tag == 8 and root == "unreferenced":
            struct.pack_into("<Q", image, position + 8, 0)
    struct.pack_into("<Q", image, RELOCATIONS, GOT + 0x4000)
    image.extend(bytes(0x9000 - len(image)))
    image[0x8800:0x9000] = image[0x4800:0x5000]
    image[0x4000:0x4100] = b"\x90" * 0x100
    struct.pack_into("<QQ", image, 64 + 32, 0x100, 0x100)
    struct.pack_into("<QQQQQ", image, 120 + 8, 0x8800, 0x8800, 0x8800, size, size)
    struct.pack_into("<IIQQQQQQ", image, 176, 1, 6, 0x8800, 0x8800, 0x8800, 0x800, 0x800, 0x1000)
    struct.pack_into("<H", image, 56, 8)
    for index in (6, 7):
        struct.pack_into("<IIQQQQQQ", image, 64 + index * 56, 0x6fffff01, 0, 0, 0, 0, 0, 0, 1)
    return image


def main():
    relinker = Path(sys.argv[1]).resolve()
    host = Path(sys.argv[2]).resolve()
    with tempfile.TemporaryDirectory(prefix="anyps5-deferred-dependencies-") as directory:
        for root in ("runtime", "executable", "bundled", "imported", "unreferenced", "noop"):
            case = Path(directory) / root
            bundled = case / "sce_module"
            plugins = case / "Media" / "Plugins"
            bundled.mkdir(parents=True)
            plugins.mkdir(parents=True)
            provider = args_module()
            strings = b"\0RecordArgs\0libc.prx\0AAAAAAAAAAA\0"
            provider[0x2600:0x2600 + len(strings)] = strings
            struct.pack_into("<Q", provider, 0x2018, len(strings))
            (plugins / "provider.prx").write_bytes(provider)
            consumer = module_with_symbol(False)
            strings = b"\0AAAAAAAAAAA#A#B\0"
            consumer[0x800:0x800 + len(strings)] = strings
            struct.pack_into("<Q", consumer, 0x618, len(strings))
            (plugins / "consumer.prx").write_bytes(page_aligned(consumer))
            (plugins / "plugin.prx").write_bytes(page_aligned(module_with_needed((b"consumer.prx",), (0,))))
            if root == "bundled":
                (bundled / "root.prx").write_bytes(page_aligned(module_with_needed((b"plugin.prx",), (0,))))
            source = case / "input.elf"
            executable = executable_with_needed(b"plugin.prx") if root == "executable" else main_fixture()
            code = bytes.fromhex("b83c000000bf2a0000000f05")
            if root in ("imported", "unreferenced", "noop"):
                executable = importing_main(root)
                if root == "imported":
                    code = b"\xff\x15" + struct.pack("<i", GOT + 0x4000 - 0x4006) + bytes.fromhex("89c7b83c0000000f05")
            executable[0x4000:0x4000 + len(code)] = code
            source.write_bytes(executable)
            output = case / "output.elf"
            result = subprocess.run([str(relinker), "--skip-syscall-check", "--module-dir", "Media/Plugins", str(source), str(output)],
                                    capture_output=True, text=True, timeout=30)
            assert result.returncode == 0, (root, result.stdout, result.stderr)
            needed = needed_libraries(output.read_bytes())
            runtime = case / "app0" / "Media" / "Plugins"
            for name in ("plugin", "consumer", "provider"):
                dependency = f"$ORIGIN/app0/Media/Plugins/{name}.prx.guest.prx"
                expected = root in ("executable", "bundled") or (root == "imported" and name == "provider")
                assert (dependency in needed) == expected, (root, name, needed)
            assert "$ORIGIN/consumer.prx.guest.prx" in needed_libraries((runtime / "plugin.prx.guest.prx").read_bytes())
            assert "$ORIGIN/provider.prx.guest.prx" in needed_libraries((runtime / "consumer.prx.guest.prx").read_bytes())
            if root in ("imported", "unreferenced", "noop"):
                macos = case / "macos"
                macos.mkdir()
                target = macos / "output.macho"
                result = subprocess.run([str(relinker), "--macos", "--skip-syscall-check", "--module-dir", "Media/Plugins", str(source), str(target)],
                                        capture_output=True, text=True, timeout=30)
                assert result.returncode == 0, (root, result.stdout, result.stderr)
                needed = MachO(target.read_bytes()).dylibs
                for name in ("plugin", "consumer", "provider"):
                    dependency = f"@executable_path/app0/Media/Plugins/{name}.prx.guest.prx"
                    expected = root == "imported" and name == "provider"
                    assert (dependency in needed) == expected, (root, name, needed)
            if sys.platform == "linux" and os.uname().machine == "x86_64":
                libraries = case / "libs"
                libraries.mkdir()
                shutil.copyfile(host, libraries / "libc.prx")
                shutil.copyfile(host, libraries / "libkernel.prx")
                events = case / "startup-events.txt"
                output.chmod(0o755)
                run = subprocess.run([str(output)], capture_output=True, text=True, timeout=30,
                                     env={**os.environ, "ANYPS5_GUEST_MODULE_ARGS": str(events)})
                assert run.returncode == 42, (root, run.returncode, run.stdout, run.stderr)
                recorded = events.read_text() if events.exists() else ""
                assert recorded.count("args=") == 0, (root, recorded)
                load_and_record(case, libraries / "libc.prx", runtime / "provider.prx.guest.prx")
    print("Guest deferred dependency tests passed")


if __name__ == "__main__":
    main()
