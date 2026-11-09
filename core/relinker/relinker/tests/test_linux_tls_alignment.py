"""Check that Linux output writes a PT_TLS p_align of 0 as 1 and keeps other alignments."""

import os
from pathlib import Path
import signal
import struct
import subprocess
import sys
import tempfile

from guest_import_fixtures import executable
from signal_run import run_without_core_dump
from test_guest_tls import static_tls_getter
from test_linux_gnu_segments import BASE, DEFAULT_BASE, executable_image, headers

PT_TLS = 7


def tls_executable(alignment):
    elf = executable_image(0x4000)
    struct.pack_into("<IIQQQQQQ", elf, 64 + 5 * 56, PT_TLS, 4, 0x5800, 0x4800, 0x4800, 8, 0x10, alignment)
    return elf


def tls_headers(data):
    return [header for header in headers(data) if header[0] == PT_TLS]


def check_executable(relinker, work, native):
    for alignment in (0, 1, 16):
        source = work / f"executable-{alignment}.elf"
        source.write_bytes(tls_executable(alignment))
        for name, options, base in (("default", [], DEFAULT_BASE), ("fixed", ["--linux-fixed-base", hex(BASE)], BASE), ("lazy", ["--lazy-binding"], 0)):
            output = work / f"executable-{alignment}-{name}.out"
            result = subprocess.run([str(relinker), "--skip-sce-module", *options, str(source), str(output)],
                                    capture_output=True, text=True, timeout=20)
            assert result.returncode == 0, (name, alignment, result.stdout, result.stderr)
            expected = [(PT_TLS, 4, 0x5800, base + 0x4800, base + 0x4800, 8, 0x10, max(alignment, 1))]
            found = tls_headers(output.read_bytes())
            assert found == expected, (name, alignment, found, expected)
            if native and name != "lazy":
                output.chmod(output.stat().st_mode | 0o111)
                execution = run_without_core_dump([str(output)], capture_output=True, text=True, timeout=20)
                assert execution.returncode == -signal.SIGTRAP, (name, alignment, execution.returncode, execution.stdout, execution.stderr)


def check_guest(relinker, work):
    for alignment in (0, 1, 8):
        case = work / f"guest-{alignment}"
        modules = case / "prx"
        modules.mkdir(parents=True)
        (modules / "tls.prx").write_bytes(static_tls_getter(alignment, 8))
        source = case / "input.elf"
        source.write_bytes(executable("tls.prx"))
        result = subprocess.run([str(relinker), str(source), str(case / "output.elf")],
                                capture_output=True, text=True, timeout=20)
        assert result.returncode == 0, ("guest", alignment, result.stdout, result.stderr)
        converted = case / "app0" / "prx" / "tls.prx.guest.prx"
        expected = [(PT_TLS, 4, 0x980, 0x2380, 0x2380, 8, 8, max(alignment, 1))]
        found = tls_headers(converted.read_bytes())
        assert found == expected, ("guest", alignment, found, expected)


def main():
    relinker = Path(sys.argv[1]).resolve()
    native = sys.platform.startswith("linux") and os.uname().machine == "x86_64"
    with tempfile.TemporaryDirectory(prefix="anyps5-linux-tls-alignment-") as directory:
        work = Path(directory)
        check_executable(relinker, work, native)
        check_guest(relinker, work)
    print("Linux TLS alignment tests passed")


if __name__ == "__main__":
    main()
