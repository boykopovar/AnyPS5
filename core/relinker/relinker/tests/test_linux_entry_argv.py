"""Check that a Linux relink reaches the guest entry and hands it the process argc and inline argv array."""

from pathlib import Path
import platform
import signal
import struct
import subprocess
import sys
import tempfile

from test_linux_load_alignment import PT_LOAD, PT_SCE_VERSION, fixture, loads

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


def far_load_fixture(address):
    image = argv_fixture()
    count, = struct.unpack_from("<H", image, 0x38)
    struct.pack_into("<H", image, 0x38, count + 1)
    struct.pack_into("<IIQQQQQQ", image, 64 + 2 * 56, PT_LOAD, 6, 0, address, address, 0, 0x1000, 0x1000)
    struct.pack_into("<IIQQQQQQ", image, 64 + count * 56, PT_SCE_VERSION, 0, 0, 0, 0, 0, 0, 1)
    return image


def entry_call_target(elf):
    entry, = struct.unpack_from("<Q", elf, 24)
    segment = next(segment for segment in loads(elf) if segment[3] <= entry < segment[3] + segment[5])
    stub = segment[2] + entry - segment[3]
    call = elf.index(0xE8, stub, stub + 32)
    displacement, = struct.unpack_from("<i", elf, call + 1)
    return entry + call - stub + 5 + displacement


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
        for address, error in ((0x7FF00000, None), (0x80000000, "Entry stub call exceeds rel32 range")):
            source.write_bytes(far_load_fixture(address))
            far = Path(directory) / ("far-%x.elf" % address)
            result = subprocess.run([str(relinker), "--skip-sce-module", str(source), str(far)],
                                    capture_output=True, text=True, timeout=20)
            if error is None:
                assert result.returncode == 0, (address, result.stdout, result.stderr)
                assert entry_call_target(far.read_bytes()) == ENTRY, hex(address)
            else:
                assert result.returncode == 2 and error in result.stderr and not far.exists(), (address, result)
    print("Linux entry argv test passed")


if __name__ == "__main__":
    main()
