import os
from pathlib import Path
import platform
import shutil
import struct
import subprocess
import sys
import tempfile

from test_guest_deferred_dependencies import page_aligned
from test_guest_export_lookup import guest_symbols
from test_guest_intel_trampolines import main_fixture
from test_guest_symbol_names import dynamic_tags, file_offset, symbol_name
from test_windows_import_modules import consumer, provider


def at_address_zero(image):
    struct.pack_into("<Q", image, 0x898 + 8, 0)
    image = page_aligned(image)
    struct.pack_into("<Q", image, 24, 0)
    struct.pack_into("<QQ", image, 64 + 16, 0, 0)
    return image


def zero_address_module(symbols):
    image = guest_symbols([(name, info, other, 1) for name, info, other in symbols])
    position = 0x600
    while True:
        tag, value = struct.unpack_from("<qQ", image, position)
        if tag == 0:
            break
        if tag == 6:
            table = value - 0x1a00
        position += 16
    for index in range(1, len(symbols) + 1):
        struct.pack_into("<Q", image, table + index * 24 + 8, 0)
    image = page_aligned(image)
    struct.pack_into("<Q", image, 24, 0)
    struct.pack_into("<QQ", image, 64 + 16, 0, 0)
    return image


def jump_target(image, value):
    offset = file_offset(image, value)
    assert image[offset] == 0xe9, image[offset:offset + 5].hex()
    displacement, = struct.unpack_from("<i", image, offset + 1)
    return value + 5 + displacement


def exports(image):
    tags = dynamic_tags(image)
    count, = struct.unpack_from("<I", image, file_offset(image, tags[4]) + 4)
    table = file_offset(image, tags[6])
    result = {}
    for index in range(1, count):
        _, info, _, section, value, _ = struct.unpack_from("<IBBHQQ", image, table + index * 24)
        if section:
            result[symbol_name(image, tags, index)] = (info, value)
    return result


def relink(relinker, case, modules):
    (case / "sce_module").mkdir(parents=True)
    for name, image in modules.items():
        (case / "sce_module" / name).write_bytes(image)
    image = main_fixture()
    code = bytes.fromhex("b83c000000bf2a0000000f05")
    image[0x4000:0x4000 + len(code)] = code
    source = case / "input.elf"
    source.write_bytes(image)
    output = case / "output.elf"
    result = subprocess.run([str(relinker), "--skip-syscall-check", str(source), str(output)],
                            capture_output=True, text=True, timeout=30)
    assert result.returncode == 0, (result.stdout, result.stderr)
    return output


def main():
    relinker = Path(sys.argv[1]).resolve()
    host = Path(sys.argv[2]).resolve()
    runnable = sys.platform.startswith("linux") and platform.machine() in ("x86_64", "AMD64")
    with tempfile.TemporaryDirectory(prefix="anyps5-linux-guest-zero-address-exports-") as directory:
        work = Path(directory)

        output = relink(relinker, work / "function", {
            "provider.prx": at_address_zero(provider(22)),
            "consumer.prx": page_aligned(consumer("provider.prx")),
        })
        image = (output.parent / "app0" / "sce_module" / "provider.prx.guest.prx").read_bytes()
        info, value = exports(image)["shared#guest"]
        assert info == 0x12 and value != 0, (info, value)
        assert jump_target(image, value) == 0, value

        output = relink(relinker, work / "kinds", {"provider.prx": zero_address_module([
            ("global", 0x12, 0), ("weak", 0x22, 0), ("protected", 0x12, 3), ("alias", 0x12, 0),
            ("internal", 0x12, 1), ("hidden", 0x12, 2), ("object", 0x11, 0)])})
        image = (output.parent / "app0" / "sce_module" / "provider.prx.guest.prx").read_bytes()
        symbols = exports(image)
        jumps = [symbols[name + "#guest"][1] for name in ("global", "weak", "protected", "alias")]
        assert all(value != 0 and jump_target(image, value) == 0 for value in jumps), symbols
        assert len(set(jumps)) == len(jumps), symbols
        assert symbols["internal"] == (0x12, 0) and symbols["hidden"] == (0x12, 0), symbols
        assert symbols["object#guest"] == (0x11, 0), symbols

        if runnable:
            libraries = work / "function" / "libs"
            libraries.mkdir()
            for name in ("libc.prx", "libkernel.prx"):
                shutil.copyfile(host, libraries / name)
            output = work / "function" / "output.elf"
            output.chmod(0o755)
            result = subprocess.run([str(output)], capture_output=True, text=True, timeout=30,
                                    env={**os.environ, "ANYPS5_GUEST_MODULE_ARGS": str(work / "events.txt")})
            assert result.returncode == 42, (result.returncode, result.stdout, result.stderr)
    print("Linux guest zero-address export tests passed")


if __name__ == "__main__":
    main()
