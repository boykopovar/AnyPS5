from pathlib import Path
import subprocess
import sys
import tempfile


def main():
    relinker = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="anyps5-help-") as directory:
        work = Path(directory)
        source = work / "missing.elf"
        output = work / "existing.exe"
        output.write_bytes(b"preserve this output")

        def run(*args):
            return subprocess.run([str(relinker), *args], cwd=work,
                                  capture_output=True, text=True, timeout=10)

        for args in [("--help",), ("--help", "--registry", "--autorun", str(source), str(output)),
                     (str(source), str(output), "--help")]:
            result = run(*args)
            assert result.returncode == 0, result.stderr
            assert result.stdout.startswith("Usage: relinker "), result.stdout
            assert "--help" in result.stdout and "Example:" in result.stdout
            assert result.stderr == "", result.stderr
            assert output.read_bytes() == b"preserve this output"
            assert sorted(path.name for path in work.iterdir()) == [output.name]

        result = run()
        assert result.returncode == 1 and "Usage: relinker" in result.stderr
        for args in [("--help", "--unknown"), ("--unknown", "--help")]:
            result = run(*args)
            assert result.returncode == 1 and "unknown option" in result.stderr
        result = run("--help", "--windows-gui")
        assert result.returncode == 1 and "requires --windows" in result.stderr
        for flag in ["--rpath", "--exclude-sce-module"]:
            result = run(flag, "--help", str(source), str(output))
            assert result.returncode == 2 and "Cannot open file" in result.stderr
            assert result.stdout == "", result.stdout
        assert output.read_bytes() == b"preserve this output"


if __name__ == "__main__":
    main()
