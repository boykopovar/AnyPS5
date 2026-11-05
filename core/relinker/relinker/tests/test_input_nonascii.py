"""Convert inputs whose directory and file names hold characters outside the local code page."""

from pathlib import Path
import subprocess
import sys
import tempfile

from test_optional_plt import fixture


def main():
    relinker = Path(sys.argv[1]).resolve()
    names = ["проба", "ゲーム", "próba m²"]
    with tempfile.TemporaryDirectory(prefix="anyps5-nonascii-") as directory:
        work = Path(directory) / "игры"
        work.mkdir()
        for name in names:
            source = work / (name + ".elf")
            source.write_bytes(bytes(fixture()))
            output = work / (name + ".exe")
            registry = work / (name + ".registry.json")
            result = subprocess.run([str(relinker), "--skip-sce-module", "--registry", "--windows", str(source), str(output)],
                                    capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=20)
            assert result.returncode == 0, (name, result)
            assert output.read_bytes()[:2] == b"MZ" and registry.exists(), (name, result)
            output.unlink()
            registry.unlink()
            source.unlink()
    print("Non-ASCII input path tests passed")


if __name__ == "__main__":
    main()
