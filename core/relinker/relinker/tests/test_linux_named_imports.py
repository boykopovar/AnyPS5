import os
from pathlib import Path
import platform
import subprocess
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parent / "macos"))

from guest_import_fixtures import executable
from nid import nid
from test_guest_module_directories import module_with_symbol, needed_libraries
from test_guest_symbol_names import imported_symbols


def library(work, name, symbol, value):
    source = work / (name + ".c")
    source.write_text("#include <unistd.h>\n"
                      f"int exported(void) __asm__(\"\\\"{symbol}\\\"\");\n"
                      f"int exported(void) {{ _exit({value}); }}\n")
    output = work / (name + ".so")
    subprocess.run([os.environ.get("CC", "cc"), "-shared", "-fPIC", str(source), "-o", str(output)], check=True)
    return output.read_bytes()


def convert(relinker, case, image, module=None):
    case.mkdir()
    if module is not None:
        (case / "sce_module").mkdir()
        (case / "sce_module" / "bundled.prx").write_bytes(module)
    source = case / "input.elf"
    source.write_bytes(image)
    output = case / "output.elf"
    options = [] if module is not None else ["--skip-sce-module"]
    result = subprocess.run([str(relinker), *options, str(source), str(output)], capture_output=True, text=True, timeout=30)
    assert result.returncode == 0, (case.name, result.stdout, result.stderr)
    return output


def run(output, library):
    (output.parent / "libs").mkdir()
    (output.parent / "libs" / "available.prx").write_bytes(library)
    output.chmod(0o755)
    return subprocess.run([str(output)], capture_output=True, text=True, timeout=30)


def main():
    relinker = Path(sys.argv[1]).resolve()
    runnable = sys.platform.startswith("linux") and platform.machine() in ("x86_64", "AMD64")
    with tempfile.TemporaryDirectory(prefix="anyps5-linux-named-imports-") as directory:
        work = Path(directory)
        cases = {
            "sprx": convert(relinker, work / "sprx", executable("available.sprx", "shared")),
            "prx": convert(relinker, work / "prx", executable("available.prx", "shared")),
            "qualified": convert(relinker, work / "qualified", executable("available.sprx", "shared#A#B")),
            "name": convert(relinker, work / "name", executable("available.sprx", "shared")),
            "guest": convert(relinker, work / "guest", executable("bundled.prx", "shared", extra_dependencies=("available.sprx",)),
                             module_with_symbol(True)),
        }
        image = cases["sprx"].read_bytes()
        assert imported_symbols(image) == {nid("shared")}, imported_symbols(image)
        assert needed_libraries(image) == ["available.prx"], needed_libraries(image)
        assert imported_symbols(cases["prx"].read_bytes()) == {"shared"}
        assert imported_symbols(cases["qualified"].read_bytes()) == {"shared"}
        image = cases["guest"].read_bytes()
        assert imported_symbols(image) == {"shared#guest"}, imported_symbols(image)
        assert "available.prx" in needed_libraries(image), needed_libraries(image)
        if not runnable:
            return
        by_nid = library(work, "by-nid", nid("shared"), 22)
        by_name = library(work, "by-name", "shared", 44)
        result = run(cases["sprx"], by_nid)
        assert result.returncode == 22, (result.returncode, result.stdout, result.stderr)
        result = run(cases["prx"], by_name)
        assert result.returncode == 44, (result.returncode, result.stdout, result.stderr)
        result = run(cases["qualified"], by_nid)
        assert result.returncode != 0 and "undefined symbol: shared" in result.stderr, (result.returncode, result.stderr)
        result = run(cases["name"], by_name)
        assert result.returncode != 0 and f"undefined symbol: {nid('shared')}" in result.stderr, (result.returncode, result.stderr)
    print("Linux named import tests passed")


if __name__ == "__main__":
    main()
