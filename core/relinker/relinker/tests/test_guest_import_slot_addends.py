from pathlib import Path
import platform
import struct
import subprocess
import sys
import tempfile

from test_guest_intel_trampolines import main_fixture, pe_sections
from test_guest_none_relocations import UNCHANGED, TARGET, module
from test_guest_symbol_names import dynamic_tags, file_offset
from test_macos_output import MachO

SECOND_TARGET = TARGET + 8
SLOT_ADDEND = 0x1234
JUMP_SLOT_ADDEND = 0x51
SYMBOL_VALUE = 0x1000
NOTE = "GLOB_DAT addends ignored per the x86-64 psABI: 1"
REJECTED = "Invalid guest symbol relocation"


def load(path):
    import ctypes
    library = ctypes.CDLL(path)
    function = getattr(library, "shared#guest")
    function.restype = ctypes.c_int
    assert function() == 42
    address = ctypes.cast(function, ctypes.c_void_p).value
    for offset in (TARGET - SYMBOL_VALUE, SECOND_TARGET - SYMBOL_VALUE):
        pointer = ctypes.c_uint64.from_address(address + offset).value
        assert pointer == address, (hex(pointer), hex(address))
    assert ctypes.CFUNCTYPE(ctypes.c_int)(pointer)() == 42


def outputs(relinker, sce, jump_slot_addend, glob_dat_addend):
    with tempfile.TemporaryDirectory(prefix="anyps5-guest-slot-addends-") as directory:
        case = Path(directory)
        modules = case / "sce_module"
        modules.mkdir()
        (modules / "module.prx").write_bytes(module([(TARGET, (1 << 32) | 6, glob_dat_addend)],
                                                    plt=[(SECOND_TARGET, (1 << 32) | 7, jump_slot_addend)], sce=sce))
        source = case / "input.elf"
        source.write_bytes(main_fixture())
        for system, flags, magic in (("linux", [], b"\x7fELF"), ("windows", ["--windows"], b"MZ"), ("macos", ["--macos"], b"\xcf\xfa\xed\xfe")):
            output = case / ("output.exe" if system == "windows" else "output.elf")
            result = subprocess.run([str(relinker), *flags, str(source), str(output)],
                                    capture_output=True, text=True, timeout=10)
            artifact = modules.parent / "app0" / "sce_module" / "module.prx.guest.prx"
            yield system, magic, result, artifact


def main():
    if sys.argv[1] == "--load":
        load(sys.argv[2])
        return
    relinker = Path(sys.argv[1]).resolve()
    for sce in (False, True):
        for system, magic, result, _ in outputs(relinker, sce, JUMP_SLOT_ADDEND, 0):
            assert result.returncode == 2 and REJECTED in result.stderr, (sce, system, result.returncode, result.stdout, result.stderr)
    for sce in (False, True):
        for system, magic, result, artifact in outputs(relinker, sce, 0, SLOT_ADDEND):
            assert result.returncode == 0, (sce, system, result.stdout, result.stderr)
            assert NOTE in result.stdout, (sce, system, result.stdout)
            data = artifact.read_bytes()
            assert data.startswith(magic), (sce, system)
            if system == "linux":
                tags = dynamic_tags(data)
                entries = []
                for tag, size in ((7, 8), (23, 2)):
                    if tags.get(size, 0):
                        start = file_offset(data, tags[tag])
                        entries.extend(struct.unpack_from("<QQq", data, position)
                                       for position in range(start, start + tags[size], 24))
                assert sorted((entry[0], entry[1] & 0xffffffff, entry[2]) for entry in entries) == \
                    [(TARGET, 6, 0), (SECOND_TARGET, 7, 0)], (sce, entries)
                assert struct.unpack_from("<Q", data, file_offset(data, TARGET))[0] == UNCHANGED, sce
                if sys.platform == "linux" and platform.machine() == "x86_64":
                    executed = subprocess.run([sys.executable, __file__, "--load", str(artifact)],
                                              capture_output=True, text=True, timeout=10)
                    assert executed.returncode == 0, (sce, executed.stdout, executed.stderr)
                continue
            if system == "windows":
                sections = pe_sections(data)
                code = next(section for section in sections if section[0] == b".elf0")
                writable = next(section for section in sections if section[0] == b".elf1")
                pe_header, = struct.unpack_from("<I", data, 0x3C)
                base, = struct.unpack_from("<Q", data, pe_header + 24 + 24)
                bound = base + code[1]
                for target in (TARGET, SECOND_TARGET):
                    value, = struct.unpack_from("<Q", data, writable[3] + target - 0x2000)
                    assert value == bound, (sce, system, hex(target), hex(value), hex(bound))
                continue
            macho = MachO(data)
            code = macho.segment("__ELF0")
            writable = macho.segment("__ELF1")
            for target in (TARGET, SECOND_TARGET):
                value, = struct.unpack_from("<Q", data, writable["fileoff"] + target - 0x2000)
                assert value == code["vmaddr"], (sce, system, hex(target), hex(value))
    print("Guest import slot addend tests passed")


if __name__ == "__main__":
    main()
