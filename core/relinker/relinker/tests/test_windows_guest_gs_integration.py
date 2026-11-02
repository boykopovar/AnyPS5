import argparse
import ctypes
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile

from test_guest_module_directories import module_with_symbol
from test_guest_intel_trampolines import main_fixture
from guest_import_fixtures import executable
from test_windows_tls_protected_entries import make_image, pe_bytes_at


GS = bytes.fromhex("65 48 a1 00 00 00 00 00 00 00 00 c3")


def executable_relocation(kind, target):
    image = make_image()
    image[0x210:0x21c] = GS
    struct.pack_into("<Q", image, 24, 0x210)
    symbol = 1 if kind in (1, 6, 7) else 0
    struct.pack_into("<IBBHQQ", image, 0x638, 1, 0x11, 0, 0, 0, 0)
    struct.pack_into("<QQq", image, 0x700, target, symbol << 32 | kind, 0x900 if kind == 8 else 0)
    tags = [(5, 0x600), (10, 10), (6, 0x620), (11, 24), (7, 0x700), (8, 24), (9, 24), (0x6100003f, 48), (0, 0)]
    for index, tag in enumerate(tags):
        struct.pack_into("<qQ", image, 0x400 + index * 16, *tag)
    struct.pack_into("<QQ", image, 120 + 32, len(tags) * 16, len(tags) * 16)
    return image


def guest_relocation(kind):
    image = module_with_symbol(True)
    image[0x400:0x40c] = GS
    struct.pack_into("<I", image, 68, 7)
    struct.pack_into("<Q", image, 0x668, 24)
    symbol = 1 if kind in (1, 6, 7) else 0
    if kind in (6, 7):
        struct.pack_into("<H", image, 0x89e, 0)
        struct.pack_into("<Q", image, 0x8a0, 0)
    struct.pack_into("<QQq", image, 0x900, 0x1003, symbol << 32 | kind, 0x2000 if kind == 8 else 0)
    if kind in (16, 17, 18):
        struct.pack_into("<H", image, 56, 4)
        struct.pack_into("<IIQQQQQQ", image, 232, 7, 4, 0x980, 0x2380, 0x2380, 8, 8, 8)
    return image


def startup_image(gs, short=False, sse=False):
    image = make_image()
    code = bytes.fromhex("48 8b 05 01 00 00 00 c3")
    if gs:
        code = b"\x65" + code
    if short:
        code = bytes.fromhex("48 8d 15 09 00 00 00 48 8d 52 01 65 48 8b 02 90 c3")
    if sse:
        code = bytes.fromhex("48 8d 15 0d 00 00 00 48 8d 52 01 65 0f 10 22 90 66 0f 7e e0 c3")
    payload = struct.pack("<Q", 42) * (2 if sse else 1)
    image[0x210:0x210 + len(code) + len(payload)] = code + payload
    struct.pack_into("<Q", image, 24, 0x210)
    struct.pack_into("<I", image, 176, 0)
    struct.pack_into("<qQ", image, 0x440, 0, 0)
    return image


def guest_initializer(short=False, sse=False):
    image = module_with_symbol(True)
    code = bytes.fromhex("65 48 8b 05 01 00 00 00 c3") + struct.pack("<Q", 42)
    image[0x400:0x400 + len(code)] = code
    if short or sse:
        image[0x400:0x407] = bytes.fromhex("8b 05 ea 13 00 00 c3")
        code = bytes.fromhex("48 8d 15 a9 13 00 00 b8 2a 00 00 00 65 89 02 90 90 c3")
        if sse:
            code = bytes.fromhex("48 8d 15 a9 13 00 00 b8 2a 00 00 00 66 44 0f 6e e0 65 f3 44 0f 11 22 c3")
        image[0x440:0x440 + len(code)] = code
    tags = []
    for offset in range(0x600, 0x700, 16):
        tag = struct.unpack_from("<qQ", image, offset)
        if tag[0] == 0:
            break
        tags.append(tag)
    tags += [(12, 0x1040 if short or sse else 0x1000), (0, 0)]
    for index, tag in enumerate(tags):
        struct.pack_into("<qQ", image, 0x600 + index * 16, *tag)
    struct.pack_into("<QQ", image, 176 + 32, len(tags) * 16, len(tags) * 16)
    return image


def skipped_gs_image():
    image = make_image()
    image[0x210:0x21a] = bytes.fromhex("eb 02 65 a4 b8 2a 00 00 00 c3")
    struct.pack_into("<Q", image, 24, 0x210)
    struct.pack_into("<I", image, 176, 0)
    struct.pack_into("<q", image, 0x710, 0x212)
    return image


def borrowed_entry_image():
    image = make_image("relative")
    image[0x210:0x219] = bytes.fromhex("65 8b 02 b8 2a 00 00 00 c3")
    struct.pack_into("<Q", image, 24, 0x213)
    struct.pack_into("<I", image, 176, 0)
    struct.pack_into("<q", image, 0x728, 0x213)
    return image


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("relinker", type=Path)
    parser.add_argument("--runtime", type=Path)
    parser.add_argument("--state-runtime", type=Path)
    parser.add_argument("--replay-runtime", type=Path)
    parser.add_argument("--dll-directory", type=Path)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="anyps5-gs-integration-") as directory:
        work = Path(directory)

        def convert(image, label, module=None):
            case = work / label
            case.mkdir()
            if module is not None:
                (case / "prx").mkdir()
                (case / "prx" / "consumer.prx").write_bytes(module)
            source = case / "input.elf"
            output = case / "output.exe"
            source.write_bytes(image)
            result = subprocess.run([str(args.relinker.resolve()), "--windows", *(["--skip-sce-module"] if module is None else []), str(source), str(output)], capture_output=True, text=True, timeout=10)
            return result, output

        for kind in (1, 6, 8, 17, 18):
            for target in (0x20b, 0x213, 0x21a):
                result, output = convert(executable_relocation(kind, target), f"executable-{kind}-{target:x}")
                assert result.returncode == 2 and "Relocation overlaps a Windows guest GS instruction" in result.stderr and not output.exists(), (kind, target, result)
        result, output = convert(executable_relocation(8, 0x300), "outside-instruction")
        assert result.returncode == 0, result
        for kind in (1, 6, 8, 16, 17, 18):
            result, output = convert(main_fixture(), f"module-{kind}", guest_relocation(kind))
            assert result.returncode == 2 and "Relocation overlaps a Windows guest GS instruction" in result.stderr and not output.exists(), (kind, result)
        if args.runtime:
            assert sys.platform == "win32" and args.dll_directory
            ctypes.windll.kernel32.SetErrorMode(3)
            for label, image, module, required in (
                ("plain", startup_image(False), None, False),
                ("skipped-gs", skipped_gs_image(), None, False),
                ("borrowed-entry", borrowed_entry_image(), None, False),
                ("gs", startup_image(True), None, True),
                ("guest-initializer", main_fixture(), guest_initializer(), True),
                ("short-gs", startup_image(True, short=True), None, True),
                ("short-guest-initializer", executable("consumer.prx"), guest_initializer(short=True), True),
                ("short-sse", startup_image(True, sse=True), None, True),
                ("sse-guest-initializer", executable("consumer.prx"), guest_initializer(sse=True), True),
            ):
                result, output = convert(image, f"startup-{label}", module)
                assert result.returncode == 0, result
                if label == "skipped-gs":
                    assert pe_bytes_at(output.read_bytes(), 0x10212, 2) == b"\x0f\x0b"
                missing = subprocess.run([str(output)], capture_output=True, text=True, timeout=10)
                if required:
                    assert missing.returncode != 42 and "libc.prx" in missing.stderr, missing
                else:
                    assert missing.returncode == 42, missing
                libraries = output.parent / "libs"
                libraries.mkdir()
                for name in ("libgcc_s_seh-1.dll", "libstdc++-6.dll", "libwinpthread-1.dll"):
                    shutil.copy2(args.dll_directory / name, libraries / name)
                if required and args.state_runtime:
                    shutil.copy2(args.state_runtime, libraries / "libc.prx")
                    state_only = subprocess.run([str(output)], capture_output=True, text=True, timeout=10)
                    assert state_only.returncode != 42 and "does not provide the Windows guest GS runtime" in state_only.stderr, (label, state_only)
                if label.startswith("short-") and args.replay_runtime:
                    shutil.copy2(args.replay_runtime, libraries / "libc.prx")
                    replay_only = subprocess.run([str(output)], capture_output=True, text=True, timeout=10)
                    assert replay_only.returncode != 42 and "does not provide the Windows guest GS runtime" in replay_only.stderr, (label, replay_only)
                shutil.copy2(args.runtime, libraries / "libc.prx")
                run = subprocess.run([str(output)], capture_output=True, text=True, timeout=10)
                assert run.returncode == 42, (label, run)
    print("Guest GS relocation and startup integration passed")


if __name__ == "__main__":
    main()
