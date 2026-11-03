from pathlib import Path
import subprocess
import sys
import tempfile

from test_guest_needed_modules import executable_with_needed
from test_guest_module_directories import module_with_symbol
from test_guest_intel_trampolines import PLAIN_SITE, guest_fixture
from test_linux_dynamic_strings import DT_RUNPATH, dynamic_strings


def main():
    relinker = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="anyps5-guest-runpath-") as directory:
        work = Path(directory)
        failures = []
        checked = 0
        for index, module_directory in enumerate(("sce_module", "prx")):
            for label, run_path in (("default", None), ("origin", "$ORIGIN"),
                                    ("custom", "$ORIGIN/host libs"), ("absolute", "/opt/absolute libs"),
                                    ("absolute-root", "/"), ("absolute-list", "/opt/libs:/usr/local/lib"),
                                    ("list", "$ORIGIN/first:$ORIGIN/second"),
                                    ("list-bare", "$ORIGIN:$ORIGIN/libs:$ORIGIN")):
                case = work / f"{index}-{label}"
                source = case / "input" / "input.elf"
                source.parent.mkdir(parents=True)
                (source.parent / "sce_module").mkdir()
                (source.parent / "sce_module" / "standard.prx").write_bytes(guest_fixture(PLAIN_SITE))
                modules = source.parent / module_directory
                modules.mkdir(parents=True, exist_ok=True)
                (modules / "needed.prx").write_bytes(module_with_symbol(True))
                source.write_bytes(executable_with_needed())
                output = case / "output" / "program.elf"
                output.parent.mkdir()
                options = [] if run_path is None else ["--rpath", run_path]
                result = subprocess.run([str(relinker), *options, str(source), str(output)],
                                        capture_output=True, text=True, timeout=20)
                assert result.returncode == 0, (result.stdout, result.stderr)
                for relative in (Path(module_directory) / "needed.prx.guest.prx", Path("sce_module/standard.prx.guest.prx")):
                    artifact = output.parent / "app0" / relative
                    tags, strings = dynamic_strings(artifact.read_bytes())
                    offset = tags[DT_RUNPATH]
                    end = strings.find(b"\0", offset)
                    assert 0 <= offset < len(strings) and end >= 0, (tags, strings)
                    actual = strings[offset:end].decode()
                    configured = run_path if run_path is not None else "$ORIGIN/libs"
                    path_list = label.startswith("list") or label == "absolute-list"
                    configured_paths = configured.split(":") if path_list else [configured]
                    actual_paths = actual.split(":") if path_list else [actual]
                    expected = [Path(path.replace("$ORIGIN", str(output.parent))).resolve() for path in configured_paths]
                    resolved = [Path(path.replace("$ORIGIN", str(artifact.parent))).resolve() for path in actual_paths]
                    if resolved != expected or (label.startswith("absolute") and actual != run_path):
                        failures.append((str(relative), label, actual, str(resolved), str(expected)))
                    checked += 1
        for index, run_path in enumerate(("relative/libs", "C:/host/libs", "C:\\host\\libs", "\\\\server\\share",
                                          "$ORIGIN/libs:", "$ORIGIN::/opt/libs", "$ORIGIN:relative/libs", "/opt/libs:")):
            output = work / f"invalid-{index}.elf"
            result = subprocess.run([str(relinker), "--rpath", run_path, str(source), str(output)],
                                    capture_output=True, text=True, timeout=20)
            assert result.returncode == 2 and "run path must be absolute or begin with $ORIGIN" in result.stderr, (
                run_path, result.returncode, result.stdout, result.stderr)
            assert not output.exists()
        output = work / "repeated.elf"
        result = subprocess.run([str(relinker), "--rpath", "$ORIGIN/a", "--rpath", "$ORIGIN/b", str(source), str(output)],
                                capture_output=True, text=True, timeout=20)
        assert result.returncode != 0 and "--rpath cannot be repeated" in result.stdout + result.stderr and not output.exists(), (
            result.returncode, result.stdout, result.stderr)
        assert not failures, failures
        print(f"Guest run path tests passed: {checked} module/path combinations")


if __name__ == "__main__":
    main()
