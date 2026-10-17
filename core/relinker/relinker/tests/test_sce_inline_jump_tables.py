from pathlib import Path
import json
import struct
import subprocess
import sys
import tempfile

PT_LOAD = 1
PT_DYNAMIC = 2
PT_GNU_EH_FRAME = 0x6474E550
PT_SCE_DYNLIBDATA = 0x61000000
FUNCTION = 0x1000
FUNCTION_SIZE = 0x9000
TABLE = FUNCTION + FUNCTION_SIZE - 8
PLT = FUNCTION + FUNCTION_SIZE
GOT = 0xA00
EH_FRAME_HEADER = 0x800
EH_FRAME = 0x840
UNSUPPORTED_TLS = bytes.fromhex("64 2d 65 6c 66 2e c3")
TABLE_ENTRIES = (-0x86F1, -0x9C)


def lea(opcode, address, target):
    return bytes.fromhex(opcode) + struct.pack("<i", target - (address + 7))


def dispatch(kind, target):
    if kind == "plain":
        return bytes.fromhex("31 c0") + lea("48 8d 0d", FUNCTION + 2, target) + bytes.fromhex("48 63 04 81 48 01 c8 ff e0")
    if kind == "copy":
        return bytes.fromhex("31 c9") + lea("48 8d 05", FUNCTION + 2, target) + bytes.fromhex("48 89 c2 48 63 04 8a 48 01 d0 ff e0")
    if kind == "r13":
        return bytes.fromhex("31 c0") + lea("4c 8d 2d", FUNCTION + 2, target) + bytes.fromhex("49 63 4c 85 00 4c 01 e9 ff e1")
    if kind == "r13-displacement":
        return bytes.fromhex("31 c0") + lea("4c 8d 2d", FUNCTION + 2, target - 8) + bytes.fromhex("49 63 4c 85 08 4c 01 e9 ff e1")
    raise ValueError(kind)


def fixture(kind):
    image = bytearray(PLT + 0x1000)
    image[:16] = b"\x7fELF\x02\x01\x01" + bytes(9)
    struct.pack_into("<HHIQQQIHHHHHH", image, 16, 3, 62, 1, FUNCTION, 64, 0, 0, 64, 56, 4, 64, 0, 0)
    tags = [
        (1, 1),
        (0x61000035, 0x600), (0x61000037, 16),
        (0x61000039, 0x620), (0x6100003B, 24), (0x6100003F, 48),
        (0x6100002F, 0x700), (0x61000031, 0), (0x61000033, 24),
        (0x61000027, GOT), (0x61000029, 0x720), (0x6100002B, 7), (0x6100002D, 24),
        (0, 0),
    ]
    struct.pack_into("<IIQQQQQQ", image, 64, PT_LOAD, 7, 0, 0, 0, len(image), len(image), 0x1000)
    struct.pack_into("<IIQQQQQQ", image, 120, PT_DYNAMIC, 6, 0x400, 0x400, 0x400, len(tags) * 16, len(tags) * 16, 8)
    struct.pack_into("<IIQQQQQQ", image, 176, PT_SCE_DYNLIBDATA, 0, 0, 0, 0, len(image), len(image), 1)
    struct.pack_into("<IIQQQQQQ", image, 232, PT_GNU_EH_FRAME, 4, EH_FRAME_HEADER, EH_FRAME_HEADER, EH_FRAME_HEADER, 20, 20, 4)
    for index, tag in enumerate(tags):
        struct.pack_into("<qQ", image, 0x400 + index * 16, *tag)
    image[0x600:0x610] = b"\x00lib.so\x00symbol\x00\x00"
    struct.pack_into("<IBBHQQ", image, 0x620 + 24, 8, 0x12, 0, 0, 0, 0)
    struct.pack_into("<QQq", image, 0x720, GOT + 24, (1 << 32) | 7, 0)

    image[FUNCTION:PLT] = b"\x90" * FUNCTION_SIZE
    if kind == "not-a-table":
        code = FUNCTION + 0x100
        body = dispatch("plain", code)
        image[code:code + len(UNSUPPORTED_TLS)] = UNSUPPORTED_TLS
    else:
        body = dispatch(kind, TABLE)
        for index, entry in enumerate(TABLE_ENTRIES):
            struct.pack_into("<i", image, TABLE + index * 4, entry)
    image[FUNCTION:FUNCTION + len(body)] = body

    image[PLT:PLT + 2] = b"\xff\x35"
    struct.pack_into("<i", image, PLT + 2, GOT + 8 - (PLT + 6))
    image[PLT + 6:PLT + 8] = b"\xff\x25"
    struct.pack_into("<i", image, PLT + 8, GOT + 16 - (PLT + 12))
    image[PLT + 12:PLT + 16] = b"\x0f\x1f\x40\x00"
    image[PLT + 16:PLT + 18] = b"\xff\x25"
    struct.pack_into("<i", image, PLT + 18, GOT + 24 - (PLT + 22))
    image[PLT + 22] = 0x68
    struct.pack_into("<I", image, PLT + 23, 0)
    image[PLT + 27] = 0xE9
    struct.pack_into("<i", image, PLT + 28, PLT - (PLT + 32))

    cie_body = b"\x00\x00\x00\x00\x01zR\x00\x01\x78\x10\x01\x1b\x00\x00\x00"
    struct.pack_into("<I", image, EH_FRAME, len(cie_body))
    image[EH_FRAME + 4:EH_FRAME + 4 + len(cie_body)] = cie_body
    fde = EH_FRAME + 4 + len(cie_body)
    struct.pack_into("<IIiiB3x", image, fde, 16, fde + 4 - EH_FRAME, FUNCTION - (fde + 8), FUNCTION_SIZE, 0)
    struct.pack_into("<I", image, fde + 20, 0)
    struct.pack_into("<BBBBiIii", image, EH_FRAME_HEADER, 1, 0x1B, 0x03, 0x3B,
                     EH_FRAME - (EH_FRAME_HEADER + 4), 1, FUNCTION - EH_FRAME_HEADER, fde - EH_FRAME_HEADER)
    return image


def run(relinker, work, name, image, target, expected_error=None):
    source = work / (name + ".elf")
    output = work / (name + ".out")
    source.write_bytes(image)
    result = subprocess.run([str(relinker), "--skip-sce-module", "--" + target, "--registry", str(source), str(output)],
                            capture_output=True, text=True, timeout=60)
    if expected_error is None:
        assert result.returncode == 0 and output.exists(), (name, result.stdout, result.stderr)
        registry = json.loads((work / (name + ".registry.json")).read_text())
        assert [[int(site, 16) for site in entry["callSites"]] for entry in registry] == [[PLT + 16]], (name, registry)
    else:
        assert result.returncode == 2 and expected_error in result.stderr and not output.exists(), (
            name, result.returncode, result.stdout, result.stderr)


def main():
    relinker = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="anyps5-sce-inline-jump-tables-") as directory:
        work = Path(directory)
        for target in ("windows", "macos"):
            for kind in ("plain", "copy", "r13"):
                run(relinker, work, kind + "-" + target, fixture(kind), target)
            run(relinker, work, "r13-displacement-" + target, fixture("r13-displacement"), target,
                "Cannot decode instruction")
            run(relinker, work, "not-a-table-" + target, fixture("not-a-table"), target,
                "Unsupported " + ("Windows" if target == "windows" else "macOS") + " guest TLS instruction")
    print("SCE inline jump table tests passed")


if __name__ == "__main__":
    main()
