"""A named import whose 8 bytes sit in the PT_TLS initialization image of a Windows executable.

The entry stub binds the import into the `.gtls` template and into the main thread's TLS block, so the
main thread and a thread created afterwards read the same address. The main thread reads its TLS
variable, calls through it and starts a thread that reads its own freshly-initialized copy.
"""

import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

import windows_thread_helper
from test_linux_load_alignment import fixture as executable_fixture
from test_tls_function_coverage import pe_bytes_at

PT_LOAD = 1
PT_TLS = 7
ENTRY = 0x200
THREAD_ENTRY = 0x280
TLS_VADDR = 0x1800
TLS_FILE_SIZE = 0x10
TLS_MEMORY_SIZE = 0x18
TLS_ALIGNMENT = 8
THREAD_POINTER = 0x18
START_SLOT = 0x1100
MAIN_VALUE = 0x1108
THREAD_VALUE = 0x1110
THREAD_FLAG = 0x1118
MAIN_RESULT = 0x111c
STRINGS = 0x600
RELA = 0x700
DYNAMIC = 0x800
SYMBOLS = 0x900
HASH = 0xa00
TARGET_VALUE = 42
MISMATCH = 43
TLS_WRITE = bytes.fromhex("65488b1425" "58000000" "488b14ca" "488982")
NAMES = ("target", "start", "helper.dll")


def strings():
    blob = bytearray([0])
    offsets = {}
    for name in NAMES:
        offsets[name] = len(blob)
        blob.extend(name.encode() + bytes([0]))
    return bytes(blob), offsets


def program():
    code = bytearray()
    fixups = []
    labels = {}

    def emit(value):
        code.extend(value)

    def label(name):
        labels[name] = ENTRY + len(code)

    def rip(opcode, target, tail=0):
        emit(bytes.fromhex(opcode))
        fixups.append((len(code), target, 4 + tail))
        emit(bytes(4))

    def short(target):
        fixups.append((len(code), target, 1))
        emit(bytes(1))

    def fs_load(displacement):
        emit(bytes.fromhex("64488b0425"))
        emit(struct.pack("<i", displacement))

    label("start")
    fs_load(-THREAD_POINTER)
    rip("488905", MAIN_VALUE)
    emit(bytes.fromhex("ffd0"))
    rip("8905", MAIN_RESULT)
    rip("488d0d", "thread")
    rip("488b05", START_SLOT)
    emit(bytes.fromhex("ffd0"))
    label("wait")
    rip("8b05", THREAD_FLAG)
    emit(bytes.fromhex("85c074"))
    short("wait")
    rip("488b05", MAIN_VALUE)
    rip("488b15", THREAD_VALUE)
    emit(bytes.fromhex("4839d075"))
    short("mismatch")
    rip("8b05", MAIN_RESULT)
    emit(bytes.fromhex("c3"))
    label("mismatch")
    emit(bytes.fromhex("b8"))
    emit(struct.pack("<I", MISMATCH))
    emit(bytes.fromhex("c3"))
    while ENTRY + len(code) < THREAD_ENTRY:
        emit(bytes.fromhex("cc"))
    label("thread")
    fs_load(-THREAD_POINTER)
    rip("488905", THREAD_VALUE)
    rip("c705", THREAD_FLAG, 4)
    emit(struct.pack("<I", 1))
    emit(bytes.fromhex("31c0c3"))
    for offset, target, length in fixups:
        destination = labels[target] if isinstance(target, str) else target
        displacement = destination - (ENTRY + offset + length)
        struct.pack_into("<b" if length == 1 else "<i", code, offset, displacement)
    return bytes(code)


def image():
    data = executable_fixture()
    struct.pack_into("<Q", data, 24, ENTRY)
    struct.pack_into("<IIQQQQQQ", data, 64 + 2 * 56, PT_LOAD, 6, 0x5000, 0x1000, 0x1000, 0x1000, 0x1000, 0x4000)
    struct.pack_into("<IIQQQQQQ", data, 64 + 3 * 56, PT_TLS, 4, 0x5800, TLS_VADDR, TLS_VADDR,
                     TLS_FILE_SIZE, TLS_MEMORY_SIZE, TLS_ALIGNMENT)
    table, offsets = strings()
    data[0x4000 + STRINGS:0x4000 + STRINGS + len(table)] = table
    tags = [(5, STRINGS), (10, len(table)), (6, SYMBOLS), (11, 24), (4, HASH), (7, RELA), (8, 48), (9, 24),
            (1, offsets["helper.dll"]), (0, 0)]
    struct.pack_into("<IIIIII", data, 0x4000 + HASH, 1, 3, 0, 0, 0, 0)
    struct.pack_into("<IIQQQQQQ", data, 120, 2, 6, 0x4000 + DYNAMIC, DYNAMIC, DYNAMIC,
                     len(tags) * 16, len(tags) * 16, 8)
    for index, tag in enumerate(tags):
        struct.pack_into("<qQ", data, 0x4000 + DYNAMIC + index * 16, *tag)
    struct.pack_into("<IBBHQQ", data, 0x4000 + SYMBOLS + 24, offsets["target"], 0x12, 0, 0, 0, 0)
    struct.pack_into("<IBBHQQ", data, 0x4000 + SYMBOLS + 48, offsets["start"], 0x12, 0, 0, 0, 0)
    struct.pack_into("<QQq", data, 0x4000 + RELA, TLS_VADDR, 1 << 32 | 1, 0)
    struct.pack_into("<QQq", data, 0x4000 + RELA + 24, START_SLOT, 2 << 32 | 6, 0)
    struct.pack_into("<QQ", data, 0x4000 + TLS_VADDR + 8, 8, THREAD_ENTRY)
    data[0x4000 + ENTRY:0x4000 + ENTRY + len(program())] = program()
    return data


def convert(relinker, case, *flags):
    (case / "libs").mkdir(parents=True)
    windows_thread_helper.write(case / "libs" / "helper.dll")
    source = case / "input.elf"
    output = case / "output.exe"
    source.write_bytes(image())
    result = subprocess.run([str(relinker), "--skip-sce-module", "--windows", *flags, str(source), str(output)],
                            capture_output=True, text=True, timeout=30)
    return result, output


def main():
    relinker = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="anyps5-windows-tls-named-imports-") as directory:
        work = Path(directory)
        for label, flags in (("bound", ()), ("lazy", ("--lazy-binding",))):
            case = work / label
            result, output = convert(relinker, case, *flags)
            assert result.returncode == 0, (result.stdout, result.stderr)
            pe = output.read_bytes()
            stub = pe_bytes_at(pe, 0x10000 + THREAD_ENTRY, 16)
            assert stub[0] == 0xE9, (label, stub.hex())
            assert pe.count(TLS_WRITE) == 1, (label, pe.count(TLS_WRITE))
            if os.name != "nt":
                continue
            run = subprocess.run([str(output)], capture_output=True, text=True, timeout=30)
            assert run.returncode == TARGET_VALUE, (label, run.returncode, run.stdout, run.stderr)
    print("Windows TLS named import tests passed")


if __name__ == "__main__":
    main()