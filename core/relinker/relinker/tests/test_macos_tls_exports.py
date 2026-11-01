"""Check `relinker --macos` on TLS exports that end at the end of PT_TLS."""
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

from test_guest_intel_trampolines import main_fixture
from test_guest_module_directories import module_with_symbol
from test_macos_executable_exports import convert, executable, exports
from test_macos_output import MachO


def guest(value, size):
    image = module_with_symbol(True)
    struct.pack_into("<H", image, 56, 4)
    struct.pack_into("<IIQQQQQQ", image, 232, 7, 4, 0x700, 0x2100, 0x2100, 0, 0x40, 16)
    struct.pack_into("<IBBHQQ", image, 0x898, 1, 0x16, 0, 1, value, size)
    return image


def tls_executable(value, size):
    return executable((("FixtureTls#A#B", 0x16, 0, 1, value, size),), tls=True)


def empty_tls_executable():
    image = tls_executable(0, 0)
    struct.pack_into("<IIQQQQQQ", image, 176, 7, 4, 0x4300, 0x300, 0x300, 0, 0, 1)
    code = bytes.fromhex("64488b042500000000b82a000000c3")
    image[0x4210:0x4210 + len(code)] = code
    return image


def convert_guest(relinker, work, name, image):
    case = work / name
    (case / "sce_module").mkdir(parents=True)
    (case / "sce_module" / "provider.prx").write_bytes(image)
    source = case / "input.elf"
    source.write_bytes(main_fixture())
    run = subprocess.run([str(relinker), "--macos", str(source), str(case / "output")],
                         capture_output=True, text=True, timeout=10)
    return run, case / "app0" / "sce_module" / "provider.prx.guest.prx"


def tls_entry(path, symbol):
    data = path.read_bytes()
    macho = MachO(data)
    meta = macho.segment("__APS5DATA")
    _, value = exports(data)[symbol]
    slot = meta["fileoff"] + value + macho.segment("__TEXT")["vmaddr"] - meta["vmaddr"]
    return struct.unpack_from("<QQ", data, slot), meta["vmaddr"] + 0x20


def main():
    relinker = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="anyps5-macos-tls-exports-") as directory:
        work = Path(directory)
        for name, value, size in (("guest-inside", 0x3F, 1), ("guest-end", 0x40, 0)):
            run, module = convert_guest(relinker, work, name, guest(value, size))
            assert run.returncode == 0, (name, run.returncode, run.stderr)
            entry, descriptor = tls_entry(module, "_shared#guest")
            assert entry == (descriptor, value), (name, entry)
        for name, image, value in (("executable-inside", tls_executable(15, 0), 15),
                                   ("executable-end", tls_executable(16, 0), 16),
                                   ("executable-empty-tls", empty_tls_executable(), 0)):
            run, output = convert(relinker, work, name, image)
            assert run.returncode == 0, (name, run.returncode, run.stderr)
            entry, descriptor = tls_entry(output, "_FixtureTls")
            assert entry == (descriptor, value), (name, entry)
        run, module = convert_guest(relinker, work, "guest-beyond", guest(0x40, 1))
        assert run.returncode == 2 and "TLS symbol exceeds TLS segment: shared" in run.stderr, (run.returncode, run.stderr)
        assert not module.exists(), module
    print("macOS TLS export tests passed")


if __name__ == "__main__":
    main()
