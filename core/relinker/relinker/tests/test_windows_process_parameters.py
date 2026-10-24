from pathlib import Path
import subprocess
import sys
import tempfile

from test_guest_intel_trampolines import main_fixture, pe_sections


def main():
    relinker = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="anyps5-windows-process-parameters-") as directory:
        work = Path(directory)
        source = work / "input.elf"
        source.write_bytes(main_fixture())
        output = work / "output.exe"
        result = subprocess.run([str(relinker), "--windows", "--skip-sce-module", str(source), str(output)],
                                capture_output=True, text=True, timeout=30)
        assert result.returncode == 0, (result.stdout, result.stderr)
        data = output.read_bytes()
        metadata = [section for section in pe_sections(data) if section[0] == b".procpar"]
        assert len(metadata) == 1, metadata
        _, _, raw_size, raw_offset, _ = metadata[0]
        stored = data[raw_offset:raw_offset + min(raw_size, 8)] if raw_offset else b""
        assert stored == bytes(len(stored)), stored
    print("Windows process parameter tests passed")


if __name__ == "__main__":
    main()
