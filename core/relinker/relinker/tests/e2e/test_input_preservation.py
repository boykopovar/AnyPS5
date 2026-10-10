import os
from pathlib import Path
import subprocess
import sys
import tempfile

from test_guest_intel_trampolines import PLAIN_SITE, guest_fixture, main_fixture


def main():
    relinker = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="anyps5-input-preservation-") as directory:
        work = Path(directory)
        for windows in (False, True):
            options = ["--windows"] if windows else []
            for bundled in (False, True):
                for alias in ("direct", "normalized", "hardlink", "symlink"):
                    case = work / f"{windows}-{bundled}-{alias}"
                    case.mkdir()
                    source = case / "input.elf"
                    source.write_bytes(main_fixture())
                    protected = source
                    if bundled:
                        modules = case / "sce_module"
                        modules.mkdir()
                        protected = modules / "sample.prx"
                        protected.write_bytes(guest_fixture(PLAIN_SITE))
                    output = protected
                    if alias == "normalized":
                        (protected.parent / "unused").mkdir()
                        output = protected.parent / "unused" / ".." / protected.name
                    elif alias == "hardlink":
                        output = case / "alias.elf"
                        os.link(protected, output)
                    elif alias == "symlink":
                        output = case / "alias.elf"
                        try:
                            output.symlink_to(protected)
                        except OSError as error:
                            if os.name != "nt" or error.winerror != 1314:
                                raise
                            print("Symbolic-link case skipped: Windows symlink privilege unavailable")
                            continue
                    original = source.read_bytes()
                    module_original = protected.read_bytes()
                    arguments = [] if bundled else ["--skip-sce-module"]
                    result = subprocess.run([str(relinker), *options, *arguments, "--registry",
                                             str(source), str(output)],
                                            capture_output=True, text=True, timeout=20)
                    expected = "Executable output would overwrite an input module" if bundled else "Executable output would overwrite the input executable"
                    if (result.returncode != 2 or expected not in result.stderr
                            or source.read_bytes() != original
                            or protected.read_bytes() != module_original
                            or output.read_bytes() != module_original
                            or output.with_suffix(".registry.json").exists()
                            or (output.parent / "app0").exists()):
                        raise AssertionError((windows, bundled, alias, result.returncode,
                                              result.stdout, result.stderr))

            for existing in (False, True):
                case = work / f"{windows}-valid-{existing}"
                case.mkdir()
                source = case / "input.elf"
                original = main_fixture()
                source.write_bytes(original)
                modules = case / "sce_module"
                modules.mkdir()
                module = modules / "sample.prx"
                module_original = guest_fixture(PLAIN_SITE)
                module.write_bytes(module_original)
                output = case / ("output.exe" if windows else "output.elf")
                if existing:
                    output.write_bytes(b"previous output")
                result = subprocess.run([str(relinker), *options, "--registry", str(source), str(output)],
                                        capture_output=True, text=True, timeout=20)
                converted = case / "app0" / "sce_module" / "sample.prx.guest.prx"
                magic = b"MZ" if windows else b"\x7fELF"
                if (result.returncode != 0 or not output.read_bytes().startswith(magic)
                        or not converted.read_bytes().startswith(magic)
                        or not output.with_suffix(".registry.json").exists()
                        or source.read_bytes() != original or module.read_bytes() != module_original):
                    raise AssertionError((windows, existing, result.returncode, result.stdout, result.stderr))
    print("Input preservation tests passed")


if __name__ == "__main__":
    main()
