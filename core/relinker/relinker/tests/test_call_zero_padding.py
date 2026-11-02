"""Convert code whose call is followed by zero padding and a function that a misaligned decoding reads as an FS access."""
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

from test_macos_output import MachO
from test_tls_function_coverage import TLS_LOAD, make_image, pe_bytes_at

NEXT_FUNCTION = bytes.fromhex("0f b6 05 49 0a 2d 00 84 c0 75 64 55 48 89 e5 5d c3")


def padded_image():
    body = TLS_LOAD + bytes.fromhex("8b 40 f0 90 90 90 e8") + struct.pack("<i", 0x12f0 - 0x1257)
    body += bytes(0x1260 - 0x1240 - len(body)) + NEXT_FUNCTION
    image = make_image("register", "symbol", 9, body)
    image[0x12f0:0x12f5] = bytes.fromhex("48 83 c4 08 c3")
    return image


def convert(relinker, work, target, image):
    source = work / f"call-zero-padding-{target}.elf"
    output = work / f"call-zero-padding-{target}.out"
    source.write_bytes(image)
    result = subprocess.run([str(relinker), "--skip-sce-module", f"--{target}", str(source), str(output)],
                            capture_output=True, text=True, timeout=60)
    assert result.returncode == 0, (target, result.stdout, result.stderr)
    return output


def main():
    relinker = Path(sys.argv[1]).resolve()
    image = padded_image()
    assert image[0x1257:0x1260] == bytes(9) and image[0x126a:0x126c] == b"\x64\x55"
    with tempfile.TemporaryDirectory(prefix="anyps5-call-zero-padding-") as directory:
        work = Path(directory)
        pe = convert(relinker, work, "windows", image).read_bytes()
        assert pe_bytes_at(pe, 0x11240, 1) == b"\xe9", "the TLS load before the call was not rewritten"
        assert pe_bytes_at(pe, 0x11260, len(NEXT_FUNCTION)) == NEXT_FUNCTION, "the function after the padding changed"
        if os.name == "nt":
            executed = subprocess.run([str(work / "call-zero-padding-windows.out")], capture_output=True, timeout=30)
            assert executed.returncode == 42, (executed.returncode, executed.stderr)
        macho = convert(relinker, work, "macos", image).read_bytes()
        code = b"".join(macho[lo:hi] for lo, hi in MachO(macho).executable_ranges())
        assert bytes(9) + NEXT_FUNCTION in code, "the function after the padding changed on macOS"
    print("Call zero padding checks passed")


if __name__ == "__main__":
    main()
