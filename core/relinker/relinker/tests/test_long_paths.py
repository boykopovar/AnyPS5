"""Convert an input and write outputs whose paths exceed the 260 characters of the Windows MAX_PATH limit."""

import os
from pathlib import Path
import subprocess
import sys
import tempfile

from test_guest_intel_trampolines import main_fixture


def long_paths_enabled():
    if os.name != "nt":
        return True
    import winreg
    try:
        with winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE, r"SYSTEM\CurrentControlSet\Control\FileSystem") as key:
            return winreg.QueryValueEx(key, "LongPathsEnabled")[0] == 1
    except OSError:
        return False


def main():
    relinker = Path(sys.argv[1]).resolve()
    if not long_paths_enabled():
        print("skipped, Windows long paths are disabled: LongPathsEnabled is not 1")
        sys.exit(77)
    with tempfile.TemporaryDirectory(prefix="anyps5-long-path-") as directory:
        work = Path(directory).joinpath(*["long-path-component-" + "x" * 40] * 5)
        work.mkdir(parents=True)
        source = work / "input.elf"
        source.write_bytes(main_fixture())
        for options, name in (([], "output.elf"), (["--windows", "--registry"], "output.exe")):
            output = work / name
            assert len(str(output)) > 260, output
            result = subprocess.run([str(relinker), "--skip-sce-module", *options, str(source), str(output)],
                                    capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=20)
            assert result.returncode == 0 and output.is_file(), (options, result)
    print("Long path tests passed")


if __name__ == "__main__":
    main()
