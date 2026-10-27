"""Check that `relinker --macos` traps guest GS accesses in place."""
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

from test_macos_output import MachO
from test_tls_function_coverage import make_image
from test_windows_tls_protected_entries import make_image as make_entry_image


FORMS = {
    "gs-load-rcx": bytes.fromhex("65 48 8b 0c 25 78 03 00 00"),
    "gs-load-r13": bytes.fromhex("65 4c 8b 2c 25 30 00 00 00"),
    "gs-store": bytes.fromhex("65 48 89 04 25 30 00 00 00"),
    "gs-alu": bytes.fromhex("65 48 33 04 25 28 00 00 00"),
    "gs-short-register-address": bytes.fromhex("65 48 8b 00"),
    "gs-maskmovdqu": bytes.fromhex("65 66 0f f7 c1"),
    "gs-maskmovq": bytes.fromhex("65 0f f7 c1"),
    "gs-vmaskmovdqu": bytes.fromhex("65 c5 f9 f7 c1"),
    "gs-monitor": bytes.fromhex("65 0f 01 c8"),
    "gs-monitorx": bytes.fromhex("65 0f 01 fa"),
    "gs-clzero": bytes.fromhex("65 0f 01 fc"),
    "gs-umonitor": bytes.fromhex("65 f3 0f ae f0"),
}

ENTRY_GS_LOAD = bytes.fromhex("65 48 8b 04 25 28 00 00 00")

NON_MEMORY_FORMS = {
    "gs-nop": bytes.fromhex("65 90"),
    "gs-nop-memory": bytes.fromhex("65 0f 1f 05 11 11 11 11"),
    "gs-lea": bytes.fromhex("65 8d 05 11 11 11 11"),
    "gs-register-move": bytes.fromhex("65 48 89 c8"),
    "gs-cpuid": bytes.fromhex("65 0f a2"),
    "gs-mwait": bytes.fromhex("65 0f 01 c9"),
    "gs-rdpru": bytes.fromhex("65 0f 01 fd"),
    "gs-umwait": bytes.fromhex("65 f2 0f ae f0"),
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


def check_refused(relinker, directory, name, body, message):
    source = Path(directory) / f"{name}.elf"
    source.write_bytes(bytes(make_image("register", "unwind", body=body)))
    result = subprocess.run([str(relinker), "--macos", "--skip-sce-module", str(source), str(Path(directory) / name)],
                            capture_output=True, text=True, timeout=60)
    if result.returncode == 0 or message not in result.stdout + result.stderr:
        raise SystemExit(f"{name}: expected the relinker to refuse with {message!r}: {result.stdout}{result.stderr}")


def convert_entry_root(relinker, directory, root):
    source = Path(directory) / f"entry-{root}.elf"
    output = Path(directory) / f"entry-{root}"
    image = make_entry_image(root)
    image[0x210:0x21a] = ENTRY_GS_LOAD + b"\xc3"
    source.write_bytes(bytes(image))
    result = subprocess.run([str(relinker), "--skip-sce-module", "--macos", "unused-filter=0", str(source), str(output)],
                            capture_output=True, text=True, timeout=60)
    return result, output


def check_entry_roots(relinker, directory):
    for root in ("relative", "symbol", "lea", "initializer"):
        result, output = convert_entry_root(relinker, directory, root)
        if (result.returncode != 2 or "GS trap would overwrite an executable entry point" not in result.stderr
                or "(offset 0x214)" not in result.stderr or output.exists()):
            raise SystemExit(f"entry-{root}: expected the GS trap to be refused at offset 0x214: "
                             f"{result.returncode} {result.stdout}{result.stderr}")
    for root in ("safe", "access-start"):
        result, output = convert_entry_root(relinker, directory, root)
        if result.returncode != 0 or not output.exists():
            raise SystemExit(f"entry-{root}: relinker failed: {result.stdout}{result.stderr}")
        data = output.read_bytes()
        code = b"".join(data[lo:hi] for lo, hi in MachO(data).executable_ranges())
        check_trapped(f"entry-{root}", code, ENTRY_GS_LOAD)


def check_untouched(name, code, instruction):
    if instruction + b"\xc3" not in code:
        raise SystemExit(f"{name}: a non-memory GS instruction was replaced")


def main():
    relinker = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="anyps5-macos-gs-") as directory:
        for name, instruction in FORMS.items():
            image = make_image("register", "unwind", body=instruction + b"\xc3")
            check_trapped(name, convert(relinker, directory, name, image), instruction)
        for name, instruction in NON_MEMORY_FORMS.items():
            image = make_image("register", "unwind", body=instruction + b"\xc3")
            check_untouched(name, convert(relinker, directory, name, image), instruction)
        instruction = FORMS["gs-load-rcx"]
        image = make_image("register", "unwind", body=instruction + b"\xc3")
        struct.pack_into("<I", image, 176, 0)
        check_trapped("gs-without-pt-tls", convert(relinker, directory, "gs-without-pt-tls", image), instruction)
        check_refused(relinker, directory, "branch-into-gs", bytes.fromhex("74 01 65 48 8b 0c 25 78 03 00 00 c3"),
                      "Branch enters a guest GS instruction")
        check_entry_roots(relinker, directory)
    print("macOS GS trap checks passed")


if __name__ == "__main__":
    main()
