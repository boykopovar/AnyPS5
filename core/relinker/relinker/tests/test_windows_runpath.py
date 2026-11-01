from pathlib import Path
import os
import subprocess
import sys
import tempfile

from test_guest_intel_trampolines import main_fixture
from test_windows_import_modules import executable, provider


def main():
    relinker = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="anyps5-windows-runpath-") as directory:
        work = Path(directory)
        seed = work / "seed"
        (seed / "prx").mkdir(parents=True)
        (seed / "prx" / "a.prx").write_bytes(provider(77))
        source = seed / "input.elf"
        source.write_bytes(main_fixture())
        result = subprocess.run([str(relinker), "--windows", str(source), str(seed / "output.exe")],
                                capture_output=True, text=True, timeout=20)
        assert result.returncode == 0, (result.stdout, result.stderr)
        library = (seed / "app0" / "prx" / "a.prx.guest.prx").read_bytes()

        for index, runpath in enumerate(("$ORIGIN", "$ORIGIN/", "$ORIGIN\\", "$ORIGIN/.", ".")):
            case = work / str(index)
            case.mkdir()
            (case / "a.prx").write_bytes(library)
            source = case / "input.elf"
            source.write_bytes(executable("a.prx"))
            output = case / "output.exe"
            result = subprocess.run([str(relinker), "--windows", "--skip-sce-module", "--rpath", runpath,
                                     str(source), str(output)], capture_output=True, text=True, timeout=20)
            assert result.returncode == 0, (runpath, result.stdout, result.stderr)
            if os.name == "nt":
                executed = subprocess.run([str(output)], cwd=work, capture_output=True, text=True, timeout=20)
                assert executed.returncode == 77, (runpath, executed.returncode, executed.stdout, executed.stderr)
    print("Windows runpath tests passed")


if __name__ == "__main__":
    main()
