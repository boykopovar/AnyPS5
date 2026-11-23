from pathlib import Path
import struct
import subprocess
import sys
import tempfile

from test_guest_intel_trampolines import main_fixture
from test_guest_symbol_names import guest_module, string_table

JUMP_SLOT = 7
RETURN_ZERO = bytes.fromhex("b800000000c3")
WHOLE_STUB = bytes.fromhex("48b82f617070302f000048890231c0c3")
SYSCALL_CODE = bytes.fromhex("31c94885d2b8c60100000f05c3")


def linked_stub(address, got_entry, index, header):
    return (b"\xff\x25" + struct.pack("<i", got_entry - (address + 6)) + b"\x68" + struct.pack("<I", index)
            + b"\xe9" + struct.pack("<i", header - (address + 16)))


def write_plt(image, file_delta, header, got, count):
    image[header - file_delta:header - file_delta + 16] = (
        b"\xff\x35" + struct.pack("<i", got + 8 - (header + 6)) + b"\xff\x25" + struct.pack("<i", got + 16 - (header + 12)) + b"\x0f\x1f\x40\x00")
    stubs = []
    for index in range(count):
        address = header + 16 * (index + 1)
        stub = linked_stub(address, got + 24 + 8 * index, index, header)
        image[address - file_delta:address - file_delta + 16] = stub
        stubs.append((address, stub))
    return stubs


def module(replacement=None, code=b"", code_address=0x10a0):
    image = guest_module([("AAAAAAAAAAA", False), ("BBBBBBBBBBB", False), ("CCCCCCCCCCC", False)])
    for index in range(3):
        struct.pack_into("<QQq", image, 0x940 + index * 24, 0x2398 + 8 * index, ((index + 1) << 32) | JUMP_SLOT, 0)
    for index, tag in enumerate([(23, 0x2340), (2, 72), (20, JUMP_SLOT), (3, 0x2380), (0, 0)], 8):
        struct.pack_into("<qQ", image, 0x600 + index * 16, *tag)
    struct.pack_into("<QQ", image, 208, 13 * 16, 13 * 16)
    stubs = write_plt(image, 0xc00, 0x1040, 0x2380, 3)
    if replacement is not None:
        image[0x1060 - 0xc00:0x1060 - 0xc00 + len(replacement)] = replacement
    image[code_address - 0xc00:code_address - 0xc00 + len(code)] = code
    return image, stubs


def provider():
    return guest_module([("AAAAAAAAAAA", True), ("BBBBBBBBBBB", True), ("CCCCCCCCCCC", True)])


def executable(replacements):
    image = main_fixture()
    struct.pack_into("<I", image, 68, 7)
    strings, offsets = string_table(["AAAAAAAAAAA", "BBBBBBBBBBB", "CCCCCCCCCCC"])
    image[0x4a00:0x4a00 + len(strings)] = strings
    for index, offset in enumerate(offsets, 1):
        struct.pack_into("<IBBHQQ", image, 0x4b00 + index * 24, offset, 0x12, 0, 0, 0, 0)
        struct.pack_into("<QQq", image, 0x4c00 + (index - 1) * 24, 0x4e18 + 8 * (index - 1), (index << 32) | JUMP_SLOT, 0)
    tags = [(5, 0x4a00), (10, len(strings)), (6, 0x4b00), (11, 24), (7, 0x4d00), (8, 0), (9, 24),
            (23, 0x4c00), (2, 72), (20, JUMP_SLOT), (3, 0x4e00), (0x6100003f, 96), (0, 0)]
    for index, tag in enumerate(tags):
        struct.pack_into("<qQ", image, 0x4800 + index * 16, *tag)
    struct.pack_into("<QQQQQ", image, 120 + 8, 0x4800, 0x4800, 0x4800, len(tags) * 16, len(tags) * 16)
    stubs = write_plt(image, 0, 0x4040, 0x4e00, 3)
    for index, replacement in replacements:
        address = stubs[index][0]
        image[address:address + len(replacement)] = replacement
    return image, stubs


def run(relinker, case, source, modules, options=()):
    case.mkdir(parents=True)
    (case / "input.elf").write_bytes(source)
    if modules:
        (case / "sce_module").mkdir()
    for name, image in modules.items():
        (case / "sce_module" / name).write_bytes(image)
    output = case / "output.elf"
    result = subprocess.run([str(relinker), *options, str(case / "input.elf"), str(output)],
                            capture_output=True, text=True, encoding="utf-8", timeout=30)
    return result, output


def reports(result):
    return [line for line in result.stdout.replace("\r", "\n").splitlines() if line.startswith("Replaced PLT stub:")]


def report(target, address, linked, found, import_name, jump=None):
    return (f"Replaced PLT stub: {target}va={address:#x} {linked.hex()} -> {found.hex()}"
            + (f" (jump to {jump:#x})" if jump is not None else "")
            + f"; import {import_name} is not called through it; a --patches entry with expected = \"{found.hex()}\""
            + f" and value = \"{linked.hex()}\" restores the linked stub")


def main():
    relinker = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="anyps5-replaced-plt-") as directory:
        work = Path(directory)

        for windows in (False, True):
            options = ["--windows"] if windows else []
            name = "windows" if windows else "linux"

            image, stubs = executable([])
            result, output = run(relinker, work / f"{name}-linked-executable", image, {"provider.prx": provider()}, options)
            assert result.returncode == 0 and not reports(result), (name, result.stdout, result.stderr)

            image, stubs = executable([(0, RETURN_ZERO), (2, WHOLE_STUB)])
            result, output = run(relinker, work / f"{name}-executable", image, {"provider.prx": provider()}, options)
            assert result.returncode == 0 and output.exists(), (name, result.stdout, result.stderr)
            assert reports(result) == [
                report("", stubs[0][0], stubs[0][1][:6], RETURN_ZERO, "AAAAAAAAAAA"),
                report("", stubs[2][0], stubs[2][1], WHOLE_STUB, "CCCCCCCCCCC"),
            ], (name, result.stdout, result.stderr)

            jump = b"\xe9" + struct.pack("<i", 0x10a0 - (0x1060 + 5)) + b"\x90"
            image, stubs = module(jump, SYSCALL_CODE)
            modules = {"sample.prx": image, "provider.prx": provider()}
            result, output = run(relinker, work / f"{name}-module", main_fixture(), modules, options)
            expected = report("sce_module/sample.prx ", 0x1060, stubs[1][1][:6], jump, "BBBBBBBBBBB", 0x10a0)
            assert result.returncode == 2 and not output.exists() and reports(result) == [expected], (name, result.stdout, result.stderr)
            assert ("Forbidden syscall instruction at code offset 0x10aa: syscall number 454; "
                    "it is in the code at 0x10a0 that the replaced PLT stub at 0x1060 jumps to") in result.stderr, (name, result.stderr)

            patches = work / f"{name}-restore.toml"
            patches.write_text(
                "format = 1\n\n"
                f"[[patch]]\ntarget = \"sce_module/sample.prx\"\nva = 0x1060\nexpected = \"{jump.hex()}\"\nvalue = \"{stubs[1][1][:6].hex()}\"\n\n"
                f"[[patch]]\ntarget = \"sce_module/sample.prx\"\nva = 0x10a0\nexpected = \"{SYSCALL_CODE.hex()}\"\nvalue = \"{'cc' * len(SYSCALL_CODE)}\"\n",
                encoding="utf-8")
            result, output = run(relinker, work / f"{name}-restored-module", main_fixture(), modules, [*options, "--patches", str(patches)])
            assert result.returncode == 0 and output.exists() and not reports(result), (name, result.stdout, result.stderr)

            image, stubs = module(jump, b"\xc3" + SYSCALL_CODE)
            modules = {"sample.prx": image, "provider.prx": provider()}
            result, output = run(relinker, work / f"{name}-returned-module", main_fixture(), modules, options)
            assert result.returncode == 2 and reports(result) == [expected], (name, result.stdout, result.stderr)
            assert "Forbidden syscall instruction at code offset 0x10ab" in result.stderr and "replaced PLT stub" not in result.stderr, (name, result.stderr)


if __name__ == "__main__":
    main()
