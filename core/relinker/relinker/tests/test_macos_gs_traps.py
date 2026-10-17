"""Check that `relinker --macos` traps guest GS accesses in place."""
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

from test_macos_output import MachO
from test_tls_function_coverage import make_image


FORMS = {
    "gs-load-rcx": bytes.fromhex("65 48 8b 0c 25 78 03 00 00"),
    "gs-load-r13": bytes.fromhex("65 4c 8b 2c 25 30 00 00 00"),
    "gs-store": bytes.fromhex("65 48 89 04 25 30 00 00 00"),
    "gs-alu": bytes.fromhex("65 48 33 04 25 28 00 00 00"),
    "gs-short-register-address": bytes.fromhex("65 48 8b 00"),
}


def convert(relinker, directory, name, image):
    source = Path(directory) / f"{name}.elf"
    output = Path(directory) / name
    source.write_bytes(bytes(image))
    result = subprocess.run([str(relinker), "--macos", "--skip-sce-module", str(source), str(output)],
                            capture_output=True, text=True, timeout=60)
    if result.returncode != 0:
        raise SystemExit(f"{name}: relinker failed: {result.stdout}{result.stderr}")
    data = output.read_bytes()
    return b"".join(data[lo:hi] for lo, hi in MachO(data).executable_ranges())


def check_trapped(name, code, instruction):
    trapped = b"\x0f\x0b" + b"\x90" * (len(instruction) - 2) + b"\xc3"
    if trapped not in code:
        raise SystemExit(f"{name}: the GS access was not replaced by ud2 and NOP padding")
    if instruction + b"\xc3" in code:
        raise SystemExit(f"{name}: the GS access was left in the code")


def main():
    relinker = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="anyps5-macos-gs-") as directory:
        for name, instruction in FORMS.items():
            image = make_image("register", "unwind", body=instruction + b"\xc3")
            check_trapped(name, convert(relinker, directory, name, image), instruction)
        instruction = FORMS["gs-load-rcx"]
        image = make_image("register", "unwind", body=instruction + b"\xc3")
        struct.pack_into("<I", image, 176, 0)
        check_trapped("gs-without-pt-tls", convert(relinker, directory, "gs-without-pt-tls", image), instruction)
    print("macOS GS trap checks passed")


if __name__ == "__main__":
    main()
