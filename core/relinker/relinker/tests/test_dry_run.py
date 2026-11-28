import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

from test_game_patches import patch_site, patch_text
from test_guest_intel_trampolines import main_fixture
from test_guest_needed_modules import executable_with_needed, module_with_needed
from test_secondary_output_collisions import prepare_secondary


def snapshot(root):
    result = {}
    for path in [root, *root.rglob("*")]:
        info = path.stat()
        result[path.relative_to(root)] = (info.st_mode, info.st_mtime_ns,
                                        path.read_bytes() if path.is_file() else None)
    return result


def run(relinker, root, arguments, dry=True, code=0, error=None, json_plan=False):
    before = snapshot(root)
    mode = "--dry-run-json" if json_plan else "--dry-run"
    result = subprocess.run([str(relinker), *([mode] if dry else []), *map(str, arguments)],
                            cwd=root, capture_output=True, text=True, encoding="utf-8", timeout=15)
    assert result.returncode == code, (result.stdout, result.stderr)
    if dry:
        assert snapshot(root) == before, "dry run changed files, directories, permissions or modification times"
        assert "Write output" not in result.stdout, result.stdout
    if error:
        assert error in result.stderr, (result.stdout, result.stderr)
        assert "Dry run complete" not in result.stdout, result.stdout
        if json_plan:
            assert not result.stdout, result.stdout
    elif dry and not json_plan:
        assert "Dry run complete" in result.stdout, result.stdout
        assert "No output files were written." in result.stdout, result.stdout
        assert "Conversion complete" not in result.stdout, result.stdout
    return result


def plan(result):
    return [Path(line.removeprefix("Would write: ")) for line in result.stdout.splitlines()
            if line.startswith("Would write: ")]


def check_plan(result, expected):
    paths = plan(result)
    assert len(paths) == len(expected) and set(paths) == set(expected), (paths, expected)


def check_json_plan(result, target, counts, expected, text_result=None):
    report = json.loads(result.stdout)
    assert report["schemaVersion"] == 1 and report["target"] == target, report
    assert (report["convertedExecutables"], report["convertedModules"], report["externalPrxReferences"]) == counts, report
    entries = [(Path(output["path"]), output["kind"]) for output in report["outputs"]]
    assert len(entries) == len(expected) and dict(entries) == expected, (entries, expected)
    assert all(path.is_absolute() for path, _ in entries), entries
    if text_result is not None:
        assert [path for path, _ in entries] == plan(text_result), entries
    assert "Converting " in result.stderr, result.stderr


def check_formats(relinker, root):
    for target in ("linux", "windows", "macos"):
        case = root / target
        case.mkdir()
        source = case / "input-\u0622.elf"
        source.write_bytes(main_fixture())
        output = case / "absent" / "nested" / "output-\u0622-\U0001f680"
        options = ["--windows", "--windows-diagnostics"] if target == "windows" else ["--macos"] if target == "macos" else []
        command = [*options, "--skip-sce-module", "--registry", source, output]
        expected = [output, output.with_suffix(".registry.json"), output.with_suffix(".needed.json")]
        if target == "windows":
            expected.append(output.parent / "windows-diagnostics-imports.txt")
        result = run(relinker, case, command)
        check_plan(result, expected)
        kinds = {
            output: "executable",
            output.with_suffix(".registry.json"): "registry",
            output.with_suffix(".needed.json"): "needed-libraries",
            output.parent / "windows-diagnostics-imports.txt": "windows-diagnostics",
        }
        expected_kinds = {path: kinds.get(path, "other") for path in expected}
        json_result = run(relinker, case, command, json_plan=True)
        check_json_plan(json_result, target, (1, 0, 0), expected_kinds, result)
        if target == "linux" and os.name != "nt":
            escaped = output.with_name('output-"\\-\t')
            json_result = run(relinker, case, [*command[:-1], escaped], json_plan=True)
            check_json_plan(json_result, target, (1, 0, 0), {
                escaped.with_name(escaped.name + path.name.removeprefix(output.name)): kind
                for path, kind in expected_kinds.items()
            })
        assert "Converted executables: 1" in result.stdout and "Converted modules: 0" in result.stdout
        output.parent.mkdir(parents=True)
        for path in expected:
            path.write_bytes(b"existing output must survive")
        run(relinker, case, command)
        run(relinker, case, command, dry=False)
        magic = b"MZ" if target == "windows" else b"\xcf\xfa\xed\xfe" if target == "macos" else b"\x7fELF"
        assert output.read_bytes().startswith(magic)
        assert not (case / "windows-diagnostics-imports.txt").exists()
        run(relinker, case, command)
        assert not list(case.rglob(".anyps5-output-identity-*"))


def check_modules(relinker, root):
    for target in ("linux", "windows", "macos"):
        case = root / target
        command, paths, _ = prepare_secondary(case, target == "windows")
        if target == "macos":
            command.insert(0, "--macos")
            paths.pop("desktop")
            paths.pop("icon")
        (case / "sce_module" / "wrapped.prx").write_bytes(bytes.fromhex("4f153d1d") + bytes(60))
        module_dir = paths["module"].parent
        module_dir.rmdir()
        module_dir.parent.rmdir()
        if target == "windows":
            output_directory = case / "out" / "nested"
            paths = {kind: output_directory / path.relative_to(case) for kind, path in paths.items()}
            command[-1] = paths["executable"]
        before_files = {path for path in case.rglob("*") if path.is_file()}
        result = run(relinker, case, command)
        check_plan(result, paths.values())
        assert "Skipping SELF container: sce_module/wrapped.prx" in result.stdout, result.stdout
        output_kinds = {
            "executable": "executable", "secondary-one": "executable", "secondary-two": "executable",
            "module": "module", "registry": "registry", "module-registry": "registry",
            "secondary-one-registry": "registry", "secondary-two-registry": "registry",
            "needed": "needed-libraries", "module-needed": "needed-libraries",
            "secondary-one-needed": "needed-libraries", "secondary-two-needed": "needed-libraries",
            "diagnostics": "windows-diagnostics", "icon": "desktop-icon", "desktop": "desktop-entry",
        }
        json_result = run(relinker, case, command, json_plan=True)
        check_json_plan(json_result, target, (3, 1, 0), {path: output_kinds.get(name, "other") for name, path in paths.items()}, result)
        assert "Executable: secondary-one.bin" in json_result.stderr, json_result.stderr
        assert "Skipping SELF container: sce_module/wrapped.prx" in json_result.stderr, json_result.stderr
        assert "Converted executables: 3" in result.stdout and "Converted modules: 1" in result.stdout
        run(relinker, case, command, dry=False)
        created = {path for path in case.rglob("*") if path.is_file()} - before_files
        assert set(plan(result)) == created, (plan(result), created)
        run(relinker, case, command)


def check_patches(relinker, root):
    source = root / "eboot.bin"
    image = main_fixture()
    source.write_bytes(image)
    address, expected = patch_site(image)
    patch = root / "relinker-patches.toml"
    patch.write_text(patch_text(address, expected, "flip_bit"), encoding="utf-8")
    output = root / "output.elf"
    manifest = root / "output.patches.toml"
    command = ["--skip-sce-module", source, output]
    result = run(relinker, root, command)
    check_plan(result, [output, manifest])
    assert "Patch: eboot.bin [flip_bit]" in result.stdout, result.stdout
    json_result = run(relinker, root, command, json_plan=True)
    check_json_plan(json_result, "linux", (1, 0, 0), {output: "executable", manifest: "patch-manifest"}, result)
    assert "Patches: " in json_result.stderr and "Patch: eboot.bin [flip_bit]" in json_result.stderr
    check_plan(run(relinker, root, ["--no-patches", *command]), [output])
    patch.rename(root / "explicit.toml")
    command = ["--patches", root / "explicit.toml", *command]
    check_plan(run(relinker, root, command), [output, manifest])
    run(relinker, root, command, dry=False)
    run(relinker, root, command)
    (root / "explicit.toml").write_text(patch_text(address, expected ^ 1, "mismatch"), encoding="utf-8")
    run(relinker, root, command, code=2, error="FAIL:")


def check_failures(relinker, root):
    source = root / "input.elf"
    source.write_bytes(main_fixture())
    output = root / "output"
    command = ["--skip-sce-module", source, output]
    before = snapshot(root)
    reader, writer = os.pipe()
    os.close(reader)
    with os.fdopen(writer, "wb") as destination:
        failed_write = subprocess.run([str(relinker), "--dry-run-json", *map(str, command)],
                                      cwd=root, stdout=destination, stderr=subprocess.PIPE, text=True,
                                      encoding="utf-8", restore_signals=False, timeout=15)
    assert failed_write.returncode == 2, failed_write.stderr
    assert "FAIL: Could not write dry-run JSON to stdout" in failed_write.stderr, failed_write.stderr
    assert "Dry run complete" not in failed_write.stderr and "Conversion complete" not in failed_write.stderr
    assert snapshot(root) == before, "failed JSON write changed the dry-run fixture"

    run(relinker, root, ["--autorun", *command], code=1, error="--dry-run conflicts with --autorun")
    run(relinker, root, ["--autorun", *command], code=1, error="--dry-run-json conflicts with --autorun", json_plan=True)
    run(relinker, root, ["--verify-patches", root / "absent.toml", source], code=1,
        error="--dry-run cannot be used with --verify-patches")
    run(relinker, root, ["--skip-sce-module", source, source], code=2, error="overwrite the input")
    output.mkdir()
    run(relinker, root, command, code=2, error="Executable output is a directory")
    output.rmdir()
    source.write_bytes(b"not an executable")
    run(relinker, root, command, code=2, error="FAIL:")
    run(relinker, root, command, code=2, error="FAIL:", json_plan=True)
    source.write_bytes(main_fixture())
    modules = root / "sce_module"
    modules.mkdir()
    module = modules / "broken.prx"
    module.write_bytes(b"\x7fELF" + bytes(64))
    run(relinker, root, [source, output], code=2, error="FAIL:")
    module.unlink()
    other = root / "other.bin"
    other.write_bytes(b"not an executable")
    run(relinker, root, ["--executable", "other.bin", source, output], code=2, error="FAIL:")
    command, paths, _ = prepare_secondary(root / "collision", False)
    os.link(paths["executable"].parent / "input.elf", paths["registry"])
    run(relinker, paths["executable"].parent, command, code=2, error="overwrite the input executable")
    paths["registry"].unlink()
    paths["registry"].write_bytes(b"existing registry")
    os.link(paths["registry"], paths["needed"])
    run(relinker, paths["executable"].parent, command, code=2, error="Output paths collide")
    run(relinker, paths["executable"].parent, command, code=2, error="Output paths collide", json_plan=True)

    bad_library = b"invalid-\xff.prx"
    for with_module in (False, True):
        case = root / ("invalid-module-registry" if with_module else "invalid-executable-registry")
        case.mkdir()
        source = case / "input.elf"
        source.write_bytes(main_fixture() if with_module else executable_with_needed(bad_library))
        if with_module:
            (case / "sce_module").mkdir()
            (case / "sce_module" / "consumer.prx").write_bytes(module_with_needed((bad_library,), (0,)))
        output = case / "output"
        output.write_bytes(b"previous executable")
        (case / "output.registry.json").write_bytes(b"previous registry")
        command = [*([] if with_module else ["--skip-sce-module"]), "--registry", source, output]
        before = snapshot(case)
        for dry in (True, False):
            run(relinker, case, command, dry=dry, code=2, error="Invalid UTF-8 lead byte in call registry string")
            assert snapshot(case) == before, "registry validation changed an output"


def main():
    relinker = Path(sys.argv[1]).resolve()
    group = sys.argv[2]
    with tempfile.TemporaryDirectory(prefix=f"anyps5-dry-run-{group}-") as directory:
        globals()[f"check_{group}"](relinker, Path(directory))
    print(f"dry run {group}: passed")


if __name__ == "__main__":
    main()
