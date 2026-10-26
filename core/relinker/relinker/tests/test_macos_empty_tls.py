"""Check `relinker --macos` on guests whose PT_TLS is empty."""
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

from test_macos_output import MachO
from test_tls_function_coverage import make_image


def empty_tls_image(body):
    image = make_image("register", "unwind", body=body + b"\xc3")
    struct.pack_into("<QQQ", image, 176 + 32, 0, 0, 1)
    image[0x1850:0x185c] = b"\x90" * 12
    return image


def convert(relinker, directory, name, image):
    source = Path(directory) / f"{name}.elf"
    output = Path(directory) / name
    source.write_bytes(bytes(image))
    result = subprocess.run([str(relinker), "--macos", "--skip-sce-module", str(source), str(output)],
                            capture_output=True, text=True, timeout=60)
    return result, output


def check_converted(relinker, directory, name, instruction):
    result, output = convert(relinker, directory, name, empty_tls_image(instruction))
    if result.returncode != 0:
        raise SystemExit(f"{name}: relinker failed: {result.stdout}{result.stderr}")
    data = output.read_bytes()
    macho = MachO(data)
    meta = struct.unpack_from("<12Q", data, macho.sections[("__APS5DATA", "__meta")][2])
    if meta[4] != 1 or meta[6] != 0 or meta[7] != 0:
        raise SystemExit(f"{name}: TLS descriptor {meta[4:9]} is not an empty block")
    code = b"".join(data[lo:hi] for lo, hi in macho.executable_ranges())
    if instruction in code:
        raise SystemExit(f"{name}: the %fs access was left in the code")


def check_tls_data_refused(relinker, directory):
    result, output = convert(relinker, directory, "empty-tls-data", empty_tls_image(bytes.fromhex("64 48 8b 04 25 f8 ff ff ff")))
    message = result.stdout + result.stderr
    if result.returncode != 2 or "is not in the thread TLS block" not in message or output.exists():
        raise SystemExit(f"empty-tls-data: a read of thread-local data with an empty PT_TLS was not refused: {message}")


def check_no_access(relinker, directory):
    result, output = convert(relinker, directory, "empty-no-access", empty_tls_image(b""))
    if result.returncode != 0:
        raise SystemExit(f"empty-no-access: relinker failed: {result.stdout}{result.stderr}")
    data = output.read_bytes()
    meta = struct.unpack_from("<12Q", data, MachO(data).sections[("__APS5DATA", "__meta")][2])
    if meta[4] != 0:
        raise SystemExit(f"empty-no-access: an empty PT_TLS without accesses got a TLS descriptor {meta[4:9]}")


def main():
    relinker = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="anyps5-macos-empty-tls-") as directory:
        check_converted(relinker, directory, "empty-guard", bytes.fromhex("64 48 8b 04 25 28 00 00 00"))
        check_converted(relinker, directory, "empty-thread-pointer", bytes.fromhex("64 48 8b 04 25 00 00 00 00"))
        check_tls_data_refused(relinker, directory)
        check_no_access(relinker, directory)
    print("macOS empty TLS checks passed")


if __name__ == "__main__":
    main()
