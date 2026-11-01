"""Check the macOS TLS scan against decodings that start inside another instruction."""
from pathlib import Path
import subprocess
import sys
import tempfile

from test_macos_output import MachO
from test_tls_function_coverage import TLS_LOAD, add_alias, make_image


def convert(relinker, directory, name, image):
    source = Path(directory) / f"{name}.elf"
    output = Path(directory) / name
    source.write_bytes(bytes(image))
    result = subprocess.run([str(relinker), "--macos", "--skip-sce-module", str(source), str(output)],
                            capture_output=True, text=True, timeout=60)
    return result, output


def check_split_function(relinker, directory):
    immediate = bytes.fromhex("48 b9 00 00 00 65 2e 62 69 6e")
    body = TLS_LOAD + bytes.fromhex("eb 00 8b 40 f0") + immediate + b"\xc3" * 8
    image = add_alias(make_image("register", "symbol", 0x55, body), 0x0c, 0x1256)
    result, output = convert(relinker, directory, "function-begins-inside-instruction", image)
    if result.returncode != 0:
        raise SystemExit(f"function-begins-inside-instruction: relinker failed: {result.stdout}{result.stderr}")
    data = output.read_bytes()
    code = b"".join(data[lo:hi] for lo, hi in MachO(data).executable_ranges())
    if immediate not in code:
        raise SystemExit("function-begins-inside-instruction: the instruction around the GS-prefixed decoding changed")


def check_gs_access_trapped(relinker, directory):
    instruction = bytes.fromhex("65 48 8b 04 25 28 00 00 00")
    result, output = convert(relinker, directory, "gs-access", make_image("register", "unwind", body=instruction + b"\xc3"))
    if result.returncode != 0:
        raise SystemExit(f"gs-access: relinker failed: {result.stdout}{result.stderr}")
    data = output.read_bytes()
    code = b"".join(data[lo:hi] for lo, hi in MachO(data).executable_ranges())
    if b"\x0f\x0b" + b"\x90" * (len(instruction) - 2) + b"\xc3" not in code:
        raise SystemExit("gs-access: a GS access at an instruction start was not trapped")


def main():
    relinker = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="anyps5-macos-tls-overlap-") as directory:
        check_split_function(relinker, directory)
        check_gs_access_trapped(relinker, directory)
    print("macOS TLS overlap checks passed")


if __name__ == "__main__":
    main()
