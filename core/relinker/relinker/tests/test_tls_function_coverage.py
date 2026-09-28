import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

from test_optional_plt import fixture


TLS_LOAD = bytes.fromhex("66 66 66 64 48 8b 04 25 00 00 00 00")


def make_image(transfer, metadata, extent=None):
    image = fixture()
    image.extend(b"\x90" * 0x1000)
    struct.pack_into("<Q", image, 24, 0x1200)
    struct.pack_into("<I", image, 68, 6)
    image[0x1200:0x1300] = b"\x90" * 0x100
    target = 0x1240
    if transfer == "table":
        code = bytes.fromhex("31 c0 48 8d 0d") + struct.pack("<i", 0x780 - 0x1209)
        code += bytes.fromhex("48 63 04 81 48 01 c8 ff e0")
        struct.pack_into("<i", image, 0x780, target - 0x780)
    elif transfer == "register":
        code = bytes.fromhex("48 8d 05") + struct.pack("<i", target - 0x1207)
        code += bytes.fromhex("ff e0")
    elif transfer == "memory":
        code = bytes.fromhex("48 8d 0d") + struct.pack("<i", target - 0x1207)
        code += bytes.fromhex("48 89 4c 24 f8 ff 64 24 f8")
    else:
        raise ValueError(transfer)
    image[0x1200:0x1200 + len(code)] = code
    body = TLS_LOAD + bytes.fromhex("8b 40 f0 c3")
    image[target:target + len(body)] = body
    image[0x1850:0x1850 + len(TLS_LOAD)] = TLS_LOAD
    struct.pack_into("<QQq", image, 0x700, 0x300, 8, 0x1200)
    struct.pack_into("<Q", image, 0x800, 42)
    struct.pack_into("<H", image, 56, 4 if metadata == "symbol" else 5)
    struct.pack_into("<IIQQQQQQ", image, 176, 7, 4, 0x800, 0x800, 0x800, 8, 16, 16)
    struct.pack_into("<IIQQQQQQ", image, 232 if metadata == "symbol" else 288, 1, 5, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000)
    function_size = target + len(body) - 0x1200 if extent is None else extent
    if metadata == "unwind":
        struct.pack_into("<IIQQQQQQ", image, 232, 0x6474e550, 4, 0x980, 0x980, 0x980, 32, 32, 8)
        struct.pack_into("<II", image, 0x900, 12, 0)
        image[0x908:0x910] = bytes.fromhex("01 00 01 78 10 00 00 00")
        struct.pack_into("<IIQQ", image, 0x910, 20, 0x14, 0x1200, function_size)
        struct.pack_into("<BBBBQIQQ", image, 0x980, 1, 0, 3, 0, 0x900, 1, 0x1200, 0x910)
    elif metadata == "symbol":
        struct.pack_into("<IBBHQQ", image, 0x638, 0, 0x12, 0, 1, 0x1200, function_size)
        struct.pack_into("<IIII", image, 0x680, 1, 2, 1, 0)
        struct.pack_into("<qQqQ", image, 0x470, 4, 0x680, 0, 0)
        struct.pack_into("<QQ", image, 160, 0x90, 8)
        struct.pack_into("<Q", image, 152, 0x90)
    else:
        raise ValueError(metadata)
    return image


def pe_bytes_at(pe, rva, size):
    header = struct.unpack_from("<I", pe, 0x3c)[0]
    count = struct.unpack_from("<H", pe, header + 6)[0]
    sections = header + 24 + struct.unpack_from("<H", pe, header + 20)[0]
    for index in range(count):
        offset = sections + index * 40
        address, length, position = struct.unpack_from("<III", pe, offset + 12)
        if address <= rva and rva + size <= address + length:
            return pe[position + rva - address:position + rva - address + size]
    raise AssertionError(f"Unmapped PE RVA {rva:#x}")


def main():
    relinker = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="anyps5-tls-coverage-") as directory:
        work = Path(directory)

        def convert(name, image, error=None, tls_address=0x1240):
            source = work / (name + ".elf")
            output = source.with_suffix(".exe")
            source.write_bytes(image)
            result = subprocess.run([str(relinker), "--windows", str(source), str(output)],
                                    capture_output=True, text=True, timeout=30)
            if error is not None:
                assert result.returncode == 2 and error in result.stderr and not output.exists(), result
                return
            assert result.returncode == 0, (name, result.stdout, result.stderr)
            pe = output.read_bytes()
            assert pe_bytes_at(pe, 0x10000 + tls_address, 1) == b"\xe9", name
            assert pe_bytes_at(pe, 0x11850, len(TLS_LOAD)) == TLS_LOAD, name
            if os.name == "nt":
                executed = subprocess.run([str(output)], capture_output=True, timeout=30)
                assert executed.returncode == 42, (name, executed.returncode, executed.stderr)

        for metadata in ("unwind", "symbol"):
            for transfer in ("table", "register", "memory"):
                convert(metadata + "-" + transfer, make_image(transfer, metadata))
            convert(metadata + "-truncated", make_image("register", metadata, 0x46), "Code analysis:")
            convert(metadata + "-outside", make_image("register", metadata, 0x1000), "Code analysis: function exceeds executable segment")
        overlapping = make_image("register", "unwind")
        overlapping[0x1200:0x1205] = b"\xe9" + struct.pack("<i", 0x1245 - 0x1205)
        convert("overlapping-entry", overlapping, "Code analysis: overlapping instruction boundaries")
        external = make_image("register", "unwind")
        external[0x1300:0x1310] = external[0x1240:0x1250]
        external[0x1240:0x1250] = b"\xe8" + struct.pack("<i", 0x1300 - 0x1245) + b"\xc3" + b"\x90" * 10
        convert("direct-call-from-indirect-block", external, tls_address=0x1300)
        conflicting = make_image("register", "symbol")
        unwind = make_image("register", "unwind", 0x51)
        conflicting[0x900:0x9a0] = unwind[0x900:0x9a0]
        conflicting[232:344] = unwind[232:344]
        struct.pack_into("<H", conflicting, 56, 5)
        convert("conflicting-function-extents", conflicting, "Code analysis: conflicting function ranges")
    print("TLS function coverage integration tests passed")


if __name__ == "__main__":
    main()
