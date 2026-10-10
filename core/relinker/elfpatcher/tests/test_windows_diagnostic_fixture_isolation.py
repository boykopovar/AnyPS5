import concurrent.futures
import ctypes
import mmap
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


def run(executable):
    result = subprocess.run([str(executable)], capture_output=True, text=True, timeout=25)
    assert result.returncode == 0, (result.returncode, result.stdout, result.stderr)
    assert "Windows dependency machine-code tests passed" in result.stdout, result.stdout


def main():
    ctypes.windll.kernel32.SetErrorMode(0x0001 | 0x0002 | 0x8000)
    original = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="anyps5-diagnostic-isolation-") as temporary:
        directory = Path(temporary)
        executable = directory / original.name
        shutil.copy2(original, executable)
        legacy = directory / "windows-diagnostic-fixtures"
        legacy.mkdir()
        fixture = legacy / "diagnostic-runner.exe"
        expected = b"previous diagnostic fixture" * 256
        fixture.write_bytes(expected)
        with fixture.open("rb") as stream, mmap.mmap(stream.fileno(), 0, access=mmap.ACCESS_READ) as mapping:
            run(executable)
            with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
                futures = [pool.submit(run, executable) for _ in range(2)]
                for future in futures:
                    future.result()
            assert mapping[:] == expected, "Mapped previous fixture changed"
            assert fixture.read_bytes() == expected, "Previous fixture was overwritten"
            assert {path.name for path in legacy.iterdir()} == {fixture.name}, "Previous fixture directory was reused"
        assert {path.name for path in directory.iterdir()} == {executable.name, legacy.name}, "Successful diagnostic runs left fixture directories"
    print("Windows diagnostic fixture isolation passed")


if __name__ == "__main__":
    main()
