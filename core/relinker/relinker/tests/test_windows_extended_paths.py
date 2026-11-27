import os
from pathlib import Path
import subprocess
import sys
import tempfile

from test_guest_intel_trampolines import main_fixture


def main():
    if os.name != "nt":
        print("skipped, Windows only")
        sys.exit(77)

    relinker = Path(sys.argv[1]).resolve()

    with tempfile.TemporaryDirectory(prefix="anyps5-extended-paths-") as directory:
        root = Path(directory).resolve()
        source = root / "input.elf"
        source.write_bytes(main_fixture())

        for extended in (False, True):
            output = root / ("extended.exe" if extended else "normal.exe")
            prefix = "\\\\?\\" if extended else ""

            result = subprocess.run(
                [str(relinker), "--skip-sce-module", "--windows", "--registry",
                 prefix + str(source), prefix + str(output)],
                capture_output=True,
                text=True,
                encoding="utf-8",
                errors="replace",
                timeout=20
            )

            assert result.returncode == 0, (
                extended, result.returncode, result.stderr
            )
            assert output.read_bytes()[:2] == b"MZ", extended
            assert output.with_suffix(".registry.json").is_file(), extended

        long_output = Path("\\\\?\\" + str(root)) / ("long output " + "x" * 225 + ".exe")
        assert len(str(long_output)) > 260
        result = subprocess.run(
            [str(relinker), "--skip-sce-module", "--windows", "--autorun", str(source), str(long_output)],
            capture_output=True, text=True, encoding="utf-8", timeout=20,
            env=dict(os.environ, ANYPS5_AUTORUN_NO_PAUSE="1"),
        )
        assert long_output.read_bytes()[:2] == b"MZ"
        assert result.returncode == 42 and "Exit code: 42" in result.stdout, (
            result.returncode, result.stdout, result.stderr
        )
        for leftover in Path("\\\\?\\" + str(root)).iterdir():
            if leftover.name.startswith("long output "):
                leftover.unlink()

    print("Windows extended path tests passed")


if __name__ == "__main__":
    main()
