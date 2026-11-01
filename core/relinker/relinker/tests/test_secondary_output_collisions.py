import itertools
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

from test_guest_intel_trampolines import main_fixture
from test_output_collisions import prepare


def unicode_destinations(relinker, work):
    if os.name != "nt":
        return
    for windows in (False, True):
        for upper, lower in (("\u00c4", "\u00e4"), ("\u03a9", "\u03c9"), ("\u0416", "\u0436"), ("\u00c4", "a")):
            case = work / f"unicode-{windows}-{ord(upper)}-{ord(lower)}"
            command, paths, _ = prepare(case, windows)
            probe = case / (upper + ".probe")
            probe.write_bytes(b"identity probe")
            alternate = case / (lower + ".probe")
            collide = alternate.exists() and os.path.samefile(probe, alternate)
            probe.unlink()
            source = case / lower / "b.bin"
            source.parent.mkdir()
            image = main_fixture()
            image[0x4001] = 43
            source.write_bytes(image)
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
                if windows:
                    for path, expected in ((output, 42), (secondary, 43)):
                        assert subprocess.run([str(path)], capture_output=True, timeout=20).returncode == expected
            assert not list(case.rglob(".anyps5-output-identity-*"))


def prepare_secondary(case, windows):
    command, paths, inputs = prepare(case, windows)
    options = []
    for name in ("secondary-one", "secondary-two"):
        source = case / f"{name}.bin"
        source.write_bytes(main_fixture())
        inputs[source] = source.read_bytes()
        options.extend(["--executable", source.name])
        paths[name] = case / f"{source.name}{paths['executable'].suffix}"
        paths[f"{name}-registry"] = case / f"{source.name}.registry.json"
        paths[f"{name}-needed"] = case / f"{source.name}.needed.json"
    if not windows:
        icon = case / "sce_sys/icon0.png"
        inputs[icon] = icon.read_bytes()
    return [*command[:-2], *options, *command[-2:]], paths, inputs


def main():
    relinker = Path(sys.argv[1]).resolve()
    secondary = {"secondary-one", "secondary-two"}
    checked = 0
    with tempfile.TemporaryDirectory(prefix="anyps5-secondary-collisions-") as directory:
        work = Path(directory)
        unicode_destinations(relinker, work)
        for windows in (False, True):
            case = work / f"{windows}-control"
            command, paths, inputs = prepare_secondary(case, windows)
            links = case / "separate-links"
            links.mkdir()
            for name, path in paths.items():
                path.write_bytes(b"previous output")
                os.link(path, links / name)
            result = subprocess.run([str(relinker), *command], cwd=case,
                                    capture_output=True, text=True, timeout=20)
            assert result.returncode == 0, (result.stdout, result.stderr)
            for name in ("executable", "module", *sorted(secondary)):
                assert paths[name].read_bytes().startswith(b"MZ" if windows else b"\x7fELF"), paths[name]
            for name, path in paths.items():
                if "registry" in name or "needed" in name:
                    json.loads(path.read_text())
            for path, original in inputs.items():
                assert path.read_bytes() == original, path
            assert not list(case.rglob(".anyps5-output-identity-*"))

            for first, second in itertools.combinations(paths, 2):
                if not secondary.intersection((first, second)):
                    continue
                case = work / f"{windows}-{first}-{second}"
                command, outputs, inputs = prepare_secondary(case, windows)
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

        for existing in (False, True):
            case = work / f"diagnostics-secondary-{existing}"
            command, paths, inputs = prepare(case, True)
            source = case / "windows-diagnostics-imports"
            source.write_bytes(main_fixture())
            inputs[source] = source.read_bytes()
            paths["executable"] = case / "output.txt"
            paths["secondary-registry"] = case / "windows-diagnostics-imports.registry.json"
            if existing:
                for path in paths.values():
                    path.write_bytes(b"previous output")
            result = subprocess.run([str(relinker), *command[:-2], "--executable", source.name,
                                     command[-2], str(paths["executable"])], cwd=case,
                                    capture_output=True, text=True, timeout=20)
            assert result.returncode == 2 and "Output paths collide" in result.stderr, (result.stdout, result.stderr)
            for path in paths.values():
                assert path.read_bytes() == b"previous output" if existing else not path.exists(), path
            for path, original in inputs.items():
                assert path.read_bytes() == original, path
            assert not list(case.rglob(".anyps5-output-identity-*"))
    print(f"Secondary output collision tests passed: {checked} hard-link pairs, direct paths, and identity controls")


if __name__ == "__main__":
    main()
