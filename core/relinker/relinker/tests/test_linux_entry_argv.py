"""Check that a Linux relink hands the guest entry the process argc and inline argv array."""

from pathlib import Path
import platform
import signal
import struct
import subprocess
import sys
import tempfile

from test_linux_load_alignment import fixture

ENTRY = 0x10
ARGV_CHECK = bytes.fromhex(
    "833f02" "751e"
    "48837f0800" "7417"
    "488b4710"
    "80385a" "750e"
    "80780100" "7508"
    "48837f1800" "7501"
    "cc"
    "0f0b")


def argv_fixture():
    image = fixture()
    struct.pack_into("<Q", image, 24, ENTRY)
    image[0x4000 + ENTRY:0x4000 + ENTRY + len(ARGV_CHECK)] = ARGV_CHECK
    return image


def main():
    relinker = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="anyps5-argv-") as directory:
        source = Path(directory) / "input.elf"
        output = Path(directory) / "output.elf"
        source.write_bytes(argv_fixture())
        result = subprocess.run([str(relinker), "--skip-sce-module", str(source), str(output)],
                                capture_output=True, text=True, timeout=20)
        assert result.returncode == 0, (result.stdout, result.stderr)
        if sys.platform.startswith("linux") and platform.machine() in ("x86_64", "AMD64"):
            output.chmod(0o755)
            for arguments, expected in ((["Z"], -signal.SIGTRAP), (["Z", "extra"], -signal.SIGILL)):
                executed = subprocess.run([str(output), *arguments], capture_output=True, timeout=20)
                assert executed.returncode == expected, (arguments, executed.returncode)
    print("Linux entry argv test passed")


if __name__ == "__main__":
    main()
