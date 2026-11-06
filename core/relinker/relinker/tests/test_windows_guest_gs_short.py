from pathlib import Path
import struct
import subprocess
import sys
import tempfile

from test_windows_guest_gs_integration import executable_relocation, guest_relocation
from test_windows_tls_protected_entries import make_image, pe_bytes_at
from test_guest_intel_trampolines import main_fixture
from test_guest_module_directories import module_with_symbol


SHORT = bytes.fromhex("65 8b 02 b8 90 90 90 90 c3")


def assert_fallback(output, rva, code):
    pe = output.read_bytes()
    assert pe_bytes_at(pe, rva, len(code)) == b"\x0f\x0b\x90" + code[3:]
    assert b"APS5GGS1" not in pe and b"APS5GGM1" not in pe and b"GuestGs" not in pe


def main():
    relinker = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="anyps5-gs-short-") as directory:
        work = Path(directory)
        number = 0

        def convert(image, module=None):
            nonlocal number
            case = work / str(number)
            number += 1
            case.mkdir()
            if module is not None:
                (case / "prx").mkdir()
                (case / "prx" / "consumer.prx").write_bytes(module)
            source = case / "input.elf"
            output = case / "output.exe"
            source.write_bytes(image)
            run = subprocess.run([str(relinker), "--windows", *(["--skip-sce-module"] if module is None else []), str(source), str(output)], capture_output=True, text=True, timeout=10)
            return run, output

        for root in ("relative", "symbol", "lea", "initializer"):
            image = make_image(root)
            image[0x210:0x210 + len(SHORT)] = SHORT
            run, output = convert(image)
            assert run.returncode == 0, (root, run)
            assert_fallback(output, 0x10210, SHORT)
        for delta in range(1, 6):
            image = make_image("relative")
            image[0x210:0x210 + len(SHORT)] = SHORT
            struct.pack_into("<q", image, 0x728, 0x210 + delta)
            run, output = convert(image)
            if delta < 3:
                assert run.returncode == 2 and "GS trap would overwrite an executable entry point" in run.stderr and not output.exists(), (delta, run)
            else:
                assert run.returncode == 0, run
                assert_fallback(output, 0x10210, SHORT)
        for delta in range(1, 6):
            image = make_image()
            image[0x210:0x210 + len(SHORT)] = SHORT
            image[0x200:0x204] = bytes((0x75, 0x10 + delta - 2, 0xeb, 0x10 - 4))
            run, output = convert(image)
            if delta < 3:
                assert run.returncode == 2 and "Branch enters a guest GS instruction" in run.stderr and not output.exists(), (delta, run)
            else:
                assert run.returncode == 0, run
                assert_fallback(output, 0x10210, SHORT)
        for kind in (1, 6, 8, 17, 18):
            for target in (0x213, 0x219):
                image = executable_relocation(kind, target)
                if kind in (1, 6):
                    image[0x60a:0x613] = b"libc.prx\0"
                    struct.pack_into("<Q", image, 0x418, 19)
                    struct.pack_into("<qQqQ", image, 0x480, 1, 10, 0, 0)
                    struct.pack_into("<QQ", image, 152, 160, 160)
                image[0x210:0x21e] = bytes.fromhex("65 8b 02 48 b8 00 00 00 00 00 00 00 00 c3")
                run, output = convert(image)
                assert run.returncode == 0, (kind, target, run)
                assert pe_bytes_at(output.read_bytes(), 0x10210, 3) == b"\x0f\x0b\x90"
                assert b"APS5GGS1" not in output.read_bytes() and b"APS5GGM1" not in output.read_bytes()
                image[0x210:0x213] = b"\x90" * 3
                reference, baseline = convert(image)
                assert reference.returncode == 0, reference
                assert pe_bytes_at(output.read_bytes(), 0x10213, 11) == pe_bytes_at(baseline.read_bytes(), 0x10213, 11)
        for kind in (1, 6, 8, 16, 17, 18):
            image = guest_relocation(kind)
            image[0x400:0x40e] = bytes.fromhex("65 8b 02 48 b8 00 00 00 00 00 00 00 00 c3")
            struct.pack_into("<Q", image, 0x900, 0x1009)
            run, output = convert(main_fixture(), image)
            assert run.returncode == 0, (kind, run)
            emitted = next(path for path in output.parent.rglob("consumer.prx.guest.prx") if path.read_bytes()[:2] == b"MZ")
            assert pe_bytes_at(emitted.read_bytes(), 0x10000, 3) == b"\x0f\x0b\x90"
            assert b"APS5GGS1" not in emitted.read_bytes() and b"APS5GGM1" not in emitted.read_bytes()
            image[0x400:0x403] = b"\x90" * 3
            reference, baseline = convert(main_fixture(), image)
            assert reference.returncode == 0, reference
            baseline = next(baseline.parent.rglob("consumer.prx.guest.prx"))
            assert pe_bytes_at(emitted.read_bytes(), 0x10003, 11) == pe_bytes_at(baseline.read_bytes(), 0x10003, 11)
        for tail in ("65 8b 04 25 00 00 00 00", "f3 48 0f ae d8 90 90 90", "65 0f 10 00 90 90 90 90"):
            code = bytes.fromhex("65 8b 02 48 b8 " + tail + " c3")
            image = make_image("relative")
            image[0x210:0x210 + len(code)] = code
            struct.pack_into("<q", image, 0x728, 0x215)
            run, output = convert(image)
            assert run.returncode == 0, (tail, run)
            assert_fallback(output, 0x10210, code)
            module = module_with_symbol(True)
            module[0x400:0x400 + len(code)] = code
            struct.pack_into("<Q", module, 0x668, 24)
            struct.pack_into("<QQq", module, 0x900, 0x2320, 8, 0x1005)
            run, output = convert(main_fixture(), module)
            assert run.returncode == 0, (tail, run)
            emitted = next(path for path in output.parent.rglob("consumer.prx.guest.prx") if path.read_bytes()[:2] == b"MZ")
            assert_fallback(emitted, 0x10000, code)
            image = make_image("relative")
            image[0x210:0x21b] = bytes.fromhex("48 b8 " + tail + " c3")
            struct.pack_into("<q", image, 0x728, 0x212)
            run, output = convert(image)
            assert run.returncode == 0, (tail, run)
            assert pe_bytes_at(output.read_bytes(), 0x10210, 11) == image[0x210:0x21b]
    print("Short guest GS entry and relocation safety passed")


if __name__ == "__main__":
    main()
