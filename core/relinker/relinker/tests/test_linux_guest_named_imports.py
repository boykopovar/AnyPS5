import os
from pathlib import Path
import platform
import shutil
import struct
import subprocess
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parent / "macos"))

from guest_import_fixtures import executable
from nid import nid
from test_guest_deferred_dependencies import page_aligned
from test_guest_intel_trampolines import main_fixture
from test_guest_module_directories import needed_libraries
from test_guest_needed_modules import module_with_needed
from test_guest_symbol_names import guest_module, imported_symbols, module_symbols, relink
from test_linux_load_alignment import fixture
from test_windows_import_modules import consumer

PROVIDER = "int provided(void) __asm__(\"\\\"%s\\\"\");\nint provided(void) { return 22; }\n"


def guest_with_needed(symbols, needed):
    image = guest_module(symbols)
    size, = struct.unpack_from("<Q", image, 0x618)
    name = needed.encode() + b"\0"
    assert size + len(name) <= 0x40
    image[0x800 + size:0x800 + size + len(name)] = name
    struct.pack_into("<Q", image, 0x618, size + len(name))
    struct.pack_into("<qQqQ", image, 0x680, 1, size, 0, 0)
    struct.pack_into("<QQ", image, 176 + 32, 10 * 16, 10 * 16)
    return image


def exporting_executable(name, info=0x12, other=0):
    image = fixture()
    strings = b"\0" + name.encode() + b"\0"
    image[0x4600:0x4600 + len(strings)] = strings
    image[0x4210:0x4216] = bytes.fromhex("b82a000000c3")
    struct.pack_into("<IBBHQQ", image, 0x4900, 0, 0, 0, 0, 0, 0)
    struct.pack_into("<IBBHQQ", image, 0x4918, 1, info, other, 1, 0x210, 6)
    tags = [(5, 0x600), (10, len(strings)), (6, 0x900), (11, 24), (7, 0x700), (8, 0), (9, 24), (0x6100003f, 48), (0, 0)]
    struct.pack_into("<IIQQQQQQ", image, 120, 2, 6, 0x4800, 0x800, 0x800, len(tags) * 16, len(tags) * 16, 8)
    for index, tag in enumerate(tags):
        struct.pack_into("<qQ", image, 0x4800 + index * 16, *tag)
    return image


def provider(work, name, symbol):
    source = work / (name + ".c")
    source.write_text(PROVIDER % symbol)
    library = work / (name + ".so")
    subprocess.run([os.environ.get("CC", "cc"), "-shared", "-fPIC", str(source), "-o", str(library)], check=True)
    return library


def run_consumer(relinker, host, case, library):
    (case / "sce_module").mkdir(parents=True)
    (case / "sce_module" / "consumer.prx").write_bytes(page_aligned(consumer("available.sprx", symbol="shared")))
    image = main_fixture()
    code = bytes.fromhex("b83c000000bf2a0000000f05")
    image[0x4000:0x4000 + len(code)] = code
    source = case / "input.elf"
    source.write_bytes(image)
    output = case / "output.elf"
    result = subprocess.run([str(relinker), "--skip-syscall-check", str(source), str(output)],
                            capture_output=True, text=True, timeout=30)
    assert result.returncode == 0, (result.stdout, result.stderr)
    module = case / "app0" / "sce_module" / "consumer.prx.guest.prx"
    assert "available.prx" in needed_libraries(module.read_bytes()), needed_libraries(module.read_bytes())
    assert nid("shared") in module_symbols(module.read_bytes()), module_symbols(module.read_bytes())
    libraries = case / "libs"
    libraries.mkdir()
    for name in ("libc.prx", "libkernel.prx"):
        shutil.copyfile(host, libraries / name)
    shutil.copyfile(library, libraries / "available.prx")
    output.chmod(0o755)
    return subprocess.run([str(output)], capture_output=True, text=True, timeout=30,
                          env={**os.environ, "ANYPS5_GUEST_MODULE_ARGS": str(case / "events.txt")})


def main():
    relinker = Path(sys.argv[1]).resolve()
    host = Path(sys.argv[2]).resolve()
    runnable = sys.platform.startswith("linux") and platform.machine() in ("x86_64", "AMD64")
    with tempfile.TemporaryDirectory(prefix="anyps5-linux-guest-named-imports-") as directory:
        work = Path(directory)

        guest = relink(relinker, work / "guest", executable("libkernel_web.sprx", "shared"), {
            "user.sprx": guest_with_needed([("shared", True), ("BBBBBBBBBBB", False), ("CCCCCCCCCCC#C#C", False)], "libkernel_web.sprx"),
            "plain.prx": guest_module([("DDDDDDDDDDD", False)]),
        })
        assert imported_symbols(guest.read_bytes()) == {"shared#guest"}, imported_symbols(guest.read_bytes())
        modules = guest.parent / "app0" / "sce_module"
        symbols = module_symbols((modules / "user.sprx.guest.prx").read_bytes())
        assert symbols == ["shared#guest", nid("BBBBBBBBBBB"), "CCCCCCCCCCC"], symbols
        symbols = module_symbols((modules / "plain.prx.guest.prx").read_bytes())
        assert symbols == ["DDDDDDDDDDD"], symbols

        dependencies = relink(relinker, work / "dependencies", executable("available.prx"), {
            "user.prx": module_with_needed((b"libSceLibcInternal.sprx", b"libkernel_web.sprx"), (0, 1)),
        })
        needed = needed_libraries(dependencies.read_bytes())
        assert needed == ["$ORIGIN/app0/sce_module/user.prx.guest.prx", "available.prx", "libSceLibcInternal.prx", "libkernel_web.prx", "libc.prx"], needed
        needed = needed_libraries((dependencies.parent / "app0" / "sce_module" / "user.prx.guest.prx").read_bytes())
        assert needed == ["libSceLibcInternal.prx", "libkernel_web.prx", "libc.prx"], needed

        for label, info, other, kept in (("global", 0x12, 0, True), ("weak", 0x22, 0, True), ("protected", 0x12, 3, True),
                                         ("internal", 0x12, 1, False), ("hidden", 0x12, 2, False), ("local", 0x02, 0, False)):
            exported = relink(relinker, work / ("executable-" + label), exporting_executable("FixtureExport#A#B", info, other), {
                "user.prx": guest_with_needed([("FixtureExport", False), ("BBBBBBBBBBB", False)], "libkernel_web.sprx"),
            })
            symbols = module_symbols((exported.parent / "app0" / "sce_module" / "user.prx.guest.prx").read_bytes())
            expected = "FixtureExport" if kept else nid("FixtureExport")
            assert symbols == [expected, nid("BBBBBBBBBBB")], (label, symbols)

        if runnable:
            result = run_consumer(relinker, host, work / "by-nid", provider(work, "by-nid", nid("shared")))
            assert result.returncode == 42, (result.returncode, result.stdout, result.stderr)
            result = run_consumer(relinker, host, work / "by-name", provider(work, "by-name", "shared"))
            assert result.returncode != 0 and nid("shared") in result.stderr, (result.returncode, result.stdout, result.stderr)
    print("Linux guest named import tests passed")


if __name__ == "__main__":
    main()
