import itertools
import json
import os
from pathlib import Path
import stat
import subprocess
import sys
import tempfile

from test_guest_intel_trampolines import PLAIN_SITE, guest_fixture, main_fixture
from test_linux_desktop_entry_output import png


def prepare(case, windows):
    case.mkdir()
    source = case / "input.elf"
    source.write_bytes(main_fixture())
    modules = case / "sce_module"
    modules.mkdir()
    module = modules / "sample.prx"
    module.write_bytes(guest_fixture(PLAIN_SITE))
    output = case / ("output.exe" if windows else "output.elf")
    paths = {
        "executable": output,
        "module": case / "app0/sce_module/sample.prx.guest.prx",
        "registry": case / "output.registry.json",
        "module-registry": case / "output.sample.prx.guest.prx.registry.json",
        "needed": case / "output.needed.json",
        "module-needed": case / "output.sample.prx.guest.prx.needed.json",
    }
    if windows:
        paths["diagnostics"] = case / "windows-diagnostics-imports.txt"
    else:
        (case / "sce_sys").mkdir()
        (case / "sce_sys/icon0.png").write_bytes(png(1, 1))
        paths["icon"] = Path(f"{output}.png")
        paths["desktop"] = Path(f"{output}.desktop")
    for path in paths.values():
        path.parent.mkdir(parents=True, exist_ok=True)
    options = ["--windows", "--windows-diagnostics"] if windows else []
    command = [*options, "--registry", str(source), str(output)]
    return command, paths, {source: source.read_bytes(), module: module.read_bytes()}


def case_sensitive(directory):
    probe = directory / "CaseProbe"
    probe.write_bytes(b"probe")
    sensitive = not (directory / "caseprobe").exists()
    probe.unlink()
    return sensitive


def main():
    relinker = Path(sys.argv[1]).resolve()
    checked = 0
    with tempfile.TemporaryDirectory(prefix="anyps5-output-collisions-") as directory:
        work = Path(directory)
        for windows, existing in itertools.product((False, True), repeat=2):
            case = work / f"needed-secondary-{windows}-{existing}"
            case.mkdir()
            source = case / "input.elf"
            secondary = case / "output.needed"
            source.write_bytes(main_fixture())
            secondary.write_bytes(main_fixture())
            module = case / "sce_module/sample.prx"
            module.parent.mkdir()
            module.write_bytes(guest_fixture(PLAIN_SITE))
            outputs = [case / name for name in ("output.json", "output.needed.json", "output.registry.json",
                                                "output.needed.registry.json", "output.needed.needed.json",
                                                "app0/sce_module/sample.prx.guest.prx",
                                                "output.sample.prx.guest.prx.registry.json",
                                                "output.sample.prx.guest.prx.needed.json")]
            if existing:
                for path in outputs:
                    path.parent.mkdir(parents=True, exist_ok=True)
                    path.write_bytes(b"previous output")
            result = subprocess.run([str(relinker), *(["--windows"] if windows else []), "--registry",
                                     "--executable", secondary.name, str(source), str(outputs[0])], cwd=case,
                                    capture_output=True, text=True, timeout=20)
            assert result.returncode == 2 and "Output paths collide" in result.stderr, (
                windows, existing, result.returncode, result.stdout, result.stderr)
            for path in outputs:
                assert path.read_bytes() == b"previous output" if existing else not path.exists(), path
            assert source.read_bytes() == secondary.read_bytes() == main_fixture()
            assert module.read_bytes() == guest_fixture(PLAIN_SITE)
        for windows in (False, True):
            command, paths, inputs = prepare(work / f"{windows}-control", windows)
            links = paths["executable"].parent / "separate-links"
            links.mkdir()
            for name, path in paths.items():
                path.write_bytes(b"previous output")
                os.link(path, links / name)
            result = subprocess.run([str(relinker), *command], cwd=paths["executable"].parent,
                                    capture_output=True, text=True, timeout=20)
            assert result.returncode == 0, (result.stdout, result.stderr)
            for name in ("executable", "module"):
                assert paths[name].read_bytes().startswith(b"MZ" if windows else b"\x7fELF")
            for name in ("registry", "module-registry", "needed", "module-needed"):
                json.loads(paths[name].read_text())
            for path, original in inputs.items():
                assert path.read_bytes() == original
            assert not list(paths["executable"].parent.rglob(".anyps5-output-identity-*"))
            for first, second in itertools.combinations(paths, 2):
                case = work / f"{windows}-{first}-{second}"
                command, outputs, inputs = prepare(case, windows)
                previous = b"previous output"
                outputs[first].write_bytes(previous)
                os.link(outputs[first], outputs[second])
                for name, path in outputs.items():
                    if name not in (first, second):
                        path.write_bytes(previous)
                result = subprocess.run([str(relinker), *command], cwd=case,
                                        capture_output=True, text=True, timeout=20)
                assert result.returncode == 2 and "Output paths collide" in result.stderr, (
                    windows, first, second, result.returncode, result.stdout, result.stderr)
                for path in outputs.values():
                    assert path.read_bytes() == previous, path
                for path, original in inputs.items():
                    assert path.read_bytes() == original, path
                assert not list(case.rglob(".anyps5-output-identity-*"))
                checked += 1
        names = ["windows-diagnostics-imports.txt"]
        if os.name == "nt" and not case_sensitive(work):
            names.append("WINDOWS-DIAGNOSTICS-IMPORTS.TXT")
        for index, name in enumerate(names):
            for existing in (False, True):
                case = work / f"diagnostics-executable-{index}-{existing}"
                case.mkdir()
                source = case / "input.elf"
                source.write_bytes(main_fixture())
                output = case / name
                if existing:
                    output.write_bytes(b"previous output")
                result = subprocess.run([str(relinker), "--windows", "--windows-diagnostics", "--skip-sce-module",
                                         str(source), str(output)], cwd=case,
                                        capture_output=True, text=True, timeout=20)
                assert result.returncode == 2 and "Output paths collide" in result.stderr, (result.stdout, result.stderr)
                assert output.read_bytes() == b"previous output" if existing else not output.exists()
                assert source.read_bytes() == main_fixture()
                assert not list(case.rglob(".anyps5-output-identity-*"))
        for existing in (False, True):
            case = work / f"case-sensitive-{existing}"
            case.mkdir()
            if not case_sensitive(case):
                print("Distinct case-variant outputs skipped: case-sensitive directory unavailable")
                break
            source = case / "input.elf"
            source.write_bytes(main_fixture())
            output = case / "WINDOWS-DIAGNOSTICS-IMPORTS.TXT"
            diagnostics = case / "windows-diagnostics-imports.txt"
            if existing:
                output.write_bytes(b"previous executable")
                diagnostics.write_bytes(b"previous diagnostics")
            result = subprocess.run([str(relinker), "--windows", "--windows-diagnostics", "--skip-sce-module",
                                     str(source), str(output)], cwd=case,
                                    capture_output=True, text=True, timeout=20)
            assert result.returncode == 0, (result.stdout, result.stderr)
            assert output.read_bytes().startswith(b"MZ") and diagnostics.exists()
            assert not os.path.samefile(output, diagnostics)
            assert not list(case.rglob(".anyps5-output-identity-*"))
        if os.name == "nt":
            case = work / "readonly-identity"
            command, paths, inputs = prepare(case, True)
            output = paths["executable"]
            output.write_bytes(b"previous output")
            os.link(output, paths["module"])
            output.chmod(stat.S_IREAD)
            try:
                result = subprocess.run([str(relinker), *command], cwd=case,
                                        capture_output=True, text=True, timeout=20)
                assert result.returncode == 2 and "Cannot compare read-only output identities" in result.stderr, (
                    result.stdout, result.stderr)
                assert output.read_bytes() == b"previous output"
                assert paths["module"].read_bytes() == b"previous output"
                assert not output.stat().st_mode & stat.S_IWRITE
                assert not list(case.rglob(".anyps5-output-identity-*"))
            finally:
                output.chmod(stat.S_IWRITE)
    print(f"Output collision tests passed: {checked} hard-link pairs, direct paths, and identity controls")


if __name__ == "__main__":
    main()
