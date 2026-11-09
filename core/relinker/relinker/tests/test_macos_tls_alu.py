"""Check that `relinker --macos` rewrites ALU instructions that read guest TLS through %fs."""
from pathlib import Path
import subprocess
import sys
import tempfile

from test_macos_output import MachO
from test_tls_function_coverage import make_image


FORMS = {
    "sub-rax-stack-guard": (bytes.fromhex("64 48 2b 04 25 28 00 00 00"), bytes.fromhex("9d 48 87 04 24 48 2b 04 24 48 8d 64 24 08")),
    "sub-rcx-stack-guard": (bytes.fromhex("64 48 2b 0c 25 28 00 00 00"), bytes.fromhex("9d 48 2b c8 58")),
    "xor-r10-stack-guard": (bytes.fromhex("64 4c 33 14 25 28 00 00 00"), bytes.fromhex("9d 4c 33 d0 58")),
    "adc-rdx-tls-block": (bytes.fromhex("64 48 13 14 25 f8 ff ff ff"), bytes.fromhex("9d 48 13 d0 58")),
    "cmp-r8-thread-pointer": (bytes.fromhex("64 4c 3b 04 25 00 00 00 00"), bytes.fromhex("9d 4c 3b c0 58")),
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


def check_outside_block_refused(relinker, directory):
    image = make_image("register", "unwind", body=bytes.fromhex("64 48 2b 04 25 00 ff ff ff c3"))
    source = Path(directory) / "outside-block.elf"
    output = Path(directory) / "outside-block"
    source.write_bytes(bytes(image))
    result = subprocess.run([str(relinker), "--macos", "--skip-sce-module", str(source), str(output)],
                            capture_output=True, text=True, timeout=60)
    message = result.stdout + result.stderr
    if result.returncode != 2 or "is not in the thread TLS block" not in message or output.exists():
        raise SystemExit(f"outside-block: an ALU read outside the thread TLS block was not refused: {message}")


def main():
    relinker = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="anyps5-macos-tls-alu-") as directory:
        for name, (instruction, operation) in FORMS.items():
            code = convert(relinker, directory, name, make_image("register", "unwind", body=instruction + b"\xc3"))
            if instruction in code:
                raise SystemExit(f"{name}: the %fs access was left in the code")
            if operation not in code:
                raise SystemExit(f"{name}: the stub does not apply the operation to the guest TLS value")
        check_outside_block_refused(relinker, directory)
    print("macOS TLS ALU checks passed")


if __name__ == "__main__":
    main()
