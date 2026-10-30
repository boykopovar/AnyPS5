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


def metadata_case(case, windows):
    command, outputs, inputs = prepare(case, windows)
    metadata = case / "sce_sys"
    metadata.mkdir(exist_ok=True)
    icon = metadata / "icon0.png"
    param = metadata / "param.json"
    icon.write_bytes(png(1, 1))
    param.write_text(json.dumps({"titleId": "PPSA00000", "defaultLanguage": "en-US",
                                "localizedParameters": {"en-US": {"titleName": "Sample Title"}}}), encoding="utf-8")
    for path in outputs.values():
        path.write_bytes(b"previous output")
    inputs.update({icon: icon.read_bytes(), param: param.read_bytes()})
    return command, outputs, inputs


def tree_snapshot(root):
    return {path.relative_to(root): ("link", os.readlink(path)) if path.is_symlink()
            else ("directory", None) if path.is_dir() else ("file", path.read_bytes())
            for path in root.rglob("*")}


def metadata_inputs(relinker, work):
    checked = 0

    def rejected(case, command):
        before = tree_snapshot(case)
        result = subprocess.run([str(relinker), *command], cwd=case,
                                capture_output=True, text=True, encoding="utf-8", timeout=20)
        assert result.returncode == 2 and "output would overwrite input metadata" in result.stderr, (
            case.name, result.returncode, result.stdout, result.stderr)
        assert tree_snapshot(case) == before, case

    for windows in (False, True):
        for name in (("icon0.png",) if windows else ("icon0.png", "param.json")):
            case = work / f"metadata-direct-{windows}-{name}"
            command, _, _ = metadata_case(case, windows)
            rejected(case, [*command[:-1], str(case / "sce_sys" / name)])
            checked += 1
            outputs = ("executable", "module", "registry", "module-registry", "needed", "module-needed")
            outputs += ("diagnostics",) if windows else ("icon", "desktop")
            for alias in ("hardlink", "symlink"):
                for output in outputs:
                    case = work / f"metadata-{windows}-{name}-{alias}-{output}"
                    command, paths, _ = metadata_case(case, windows)
                    destination = paths[output]
                    destination.unlink()
                    metadata = case / "sce_sys" / name
                    if alias == "hardlink":
                        os.link(metadata, destination)
                    else:
                        try:
                            destination.symlink_to(metadata)
                        except (OSError, NotImplementedError) as error:
                            print(f"Metadata symlink case skipped: {error}")
                            continue
                    rejected(case, command)
                    checked += 1
        case = work / f"metadata-distinct-{windows}"
        command, paths, inputs = metadata_case(case, windows)
        result = subprocess.run([str(relinker), *command], cwd=case,
                                capture_output=True, text=True, encoding="utf-8", timeout=20)
        assert result.returncode == 0, (result.stdout, result.stderr)
        for path, original in inputs.items():
            assert path.read_bytes() == original, path
        assert paths["executable"].read_bytes().startswith(b"MZ" if windows else b"\x7fELF")

    for format, name in (("windows", "param.json"), ("macos", "icon0.png"), ("linux-no-icon", "param.json")):
        case = work / f"metadata-unused-{format}"
        command, _, inputs = metadata_case(case, format == "windows")
        if format == "macos":
            command.insert(0, "--macos")
        if format == "linux-no-icon":
            icon = case / "sce_sys/icon0.png"
            icon.unlink()
            inputs.pop(icon)
        output = case / "sce_sys" / name
        result = subprocess.run([str(relinker), *command[:-1], str(output)], cwd=case,
                                capture_output=True, text=True, encoding="utf-8", timeout=20)
        assert result.returncode == 0, (format, result.stdout, result.stderr)
        magic = b"MZ" if format == "windows" else b"\xcf\xfa\xed\xfe" if format == "macos" else b"\x7fELF"
        assert output.read_bytes().startswith(magic), output
        for path, original in inputs.items():
            if path != output:
                assert path.read_bytes() == original, path
    return checked


def filesystem_aliases(relinker, work):
    if os.name == "nt":
        return
    for windows in (False, True):
        for upper, lower in (("A", "a"), ("\u00c4", "\u00e4"), ("\u03a9", "\u03c9"), ("\u0416", "\u0436"),
                             ("\u00e9", "e\u0301"), ("\u00c4", "a")):
            case = work / f"alias-{windows}-{ord(upper)}-{ord(lower[0])}-{len(lower)}"
            command, paths, _ = prepare(case, windows)
            probe = case / (upper + ".probe")
            probe.write_bytes(b"identity probe")
            alternate = case / (lower + ".probe")
            collide = alternate.exists() and os.path.samefile(probe, alternate)
            probe.unlink()
            source = case / lower / "b.bin"
            source.parent.mkdir()
            source.write_bytes(main_fixture())
            suffix = paths["executable"].suffix
            output = case / (upper + ".b.bin" + suffix)
            secondary = case / (lower + ".b.bin" + suffix)
            inputs = {path: path.read_bytes() for path in case.rglob("*") if path.is_file()}
            result = subprocess.run([str(relinker), *command[:-2], "--executable", f"{lower}/b.bin",
                                     command[-2], str(output)], cwd=case,
                                    capture_output=True, text=True, encoding="utf-8", timeout=20)
            for path, original in inputs.items():
                assert path.read_bytes() == original, path
            if collide:
                assert result.returncode == 2 and "Output paths collide" in result.stderr, (
                    windows, upper, lower, result.stdout, result.stderr)
                assert {path for path in case.rglob("*") if path.is_file()} == set(inputs)
            else:
                assert result.returncode == 0 and output.exists() and secondary.exists(), (
                    windows, upper, lower, result.stdout, result.stderr)
                assert not os.path.samefile(output, secondary)
            assert not list(case.rglob(".anyps5-output-identity-*"))


def directory_outputs(relinker, work):
    labels = {"module": "Guest module", "registry": "Registry", "module-registry": "Registry",
              "needed": "Needed libraries", "module-needed": "Needed libraries", "diagnostics": "Windows import diagnostics",
              "icon": "Icon", "desktop": "Desktop entry", "extra": "Executable"}
    for windows in (False, True):
        keys = ("module", "registry", "module-registry", "needed", "module-needed")
        keys += ("diagnostics",) if windows else ("icon", "desktop")
        for key in (*keys, "extra"):
            case = work / f"directory-{windows}-{key}"
            command, paths, _ = prepare(case, windows)
            if key == "extra":
                source = case / "extra/b.bin"
                source.parent.mkdir()
                source.write_bytes(main_fixture())
                paths[key] = case / ("extra.b.bin" + paths["executable"].suffix)
                command = [*command[:-2], "--executable", "extra/b.bin", command[-2], command[-1]]
            paths[key].mkdir()
            before = tree_snapshot(case)
            result = subprocess.run([str(relinker), *command], cwd=case,
                                    capture_output=True, text=True, encoding="utf-8", timeout=20)
            assert result.returncode == 2 and f"FAIL: {labels[key]} output is a directory: " in result.stderr, (
                windows, key, result.returncode, result.stdout, result.stderr)
            assert paths[key].name in result.stderr, (windows, key, result.stderr)
            assert tree_snapshot(case) == before, (windows, key)


def main():
    relinker = Path(sys.argv[1]).resolve()
    checked = 0
    with tempfile.TemporaryDirectory(prefix="anyps5-output-collisions-") as directory:
        work = Path(directory)
        metadata_checked = metadata_inputs(relinker, work)
        filesystem_aliases(relinker, work)
        directory_outputs(relinker, work)
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
        if not case_sensitive(work):
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
                assert result.returncode == 2 and "Output paths collide" in result.stderr, (
                    result.stdout, result.stderr)
                assert output.read_bytes() == b"previous output"
                assert paths["module"].read_bytes() == b"previous output"
                assert not output.stat().st_mode & stat.S_IWRITE
                assert not list(case.rglob(".anyps5-output-identity-*"))
            finally:
                output.chmod(stat.S_IWRITE)
    print(f"Output collision tests passed: {checked} hard-link pairs, {metadata_checked} metadata protections, direct paths, and identity controls")


if __name__ == "__main__":
    main()
