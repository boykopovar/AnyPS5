"""Validate the --windows-gui PE subsystem flag through the real ELF-to-PE pipeline."""

from pathlib import Path
import struct
import subprocess
import sys
import tempfile

from test_optional_plt import fixture


def subsystem(pe):
    pe_offset = struct.unpack_from("<I", pe, 0x3c)[0]
    return struct.unpack_from("<H", pe, pe_offset + 24 + 68)[0]


def main():
    relinker = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="anyps5-gui-") as directory:
        work = Path(directory)
        source = work / "input.elf"
        source.write_bytes(fixture())

        cui_output = work / "cui.exe"
        result = subprocess.run([str(relinker), "--skip-sce-module", "--windows", str(source), str(cui_output)],
                                capture_output=True, text=True, timeout=20)
        assert result.returncode == 0, (result.stdout, result.stderr)
        assert subsystem(cui_output.read_bytes()) == 3, "expected IMAGE_SUBSYSTEM_WINDOWS_CUI by default"

        gui_output = work / "gui.exe"
        result = subprocess.run([str(relinker), "--skip-sce-module", "--windows", "--windows-gui", str(source), str(gui_output)],
                                capture_output=True, text=True, timeout=20)
        assert result.returncode == 0, (result.stdout, result.stderr)
        assert subsystem(gui_output.read_bytes()) == 2, "expected IMAGE_SUBSYSTEM_WINDOWS_GUI with --windows-gui"

        bad_output = work / "bad.exe"
        result = subprocess.run([str(relinker), "--skip-sce-module", "--windows-gui", str(source), str(bad_output)],
                                capture_output=True, text=True, timeout=20)
        assert result.returncode != 0 and not bad_output.exists(), result
    print("Windows GUI subsystem integration tests passed")


if __name__ == "__main__":
    main()
