import os
from pathlib import Path
import subprocess
import sys
import tempfile

from test_game_patches import patch_site, patch_text
from test_guest_intel_trampolines import main_fixture
from test_secondary_output_collisions import prepare_secondary


def snapshot(root):
    result = {}
    for path in [root, *root.rglob("*")]:
        info = path.stat()
        result[path.relative_to(root)] = (info.st_mode, info.st_mtime_ns,
                                        path.read_bytes() if path.is_file() else None)
    return result


def run(relinker, root, arguments, dry=True, code=0, error=None):
    before = snapshot(root)
    result = subprocess.run([str(relinker), *(["--dry-run"] if dry else []), *map(str, arguments)],
                            cwd=root, capture_output=True, text=True, encoding="utf-8", timeout=15)
    assert result.returncode == code, (result.stdout, result.stderr)
    if dry:
        assert snapshot(root) == before, "dry run changed files, directories, permissions or modification times"
        assert "Write output" not in result.stdout, result.stdout
    if error:
        assert error in result.stderr, (result.stdout, result.stderr)
        assert "Dry run complete" not in result.stdout, result.stdout
    elif dry:
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


def check_formats(relinker, root):
    for target in ("linux", "windows", "macos"):
        case = root / target
        case.mkdir()
        source = case / "input-\u0622.elf"
        source.write_bytes(main_fixture())
        output = case / "absent" / "nested" / "output"
        options = ["--windows", "--windows-diagnostics"] if target == "windows" else ["--macos"] if target == "macos" else []
        command = [*options, "--skip-sce-module", "--registry", source, output]
        expected = [output, output.with_suffix(".registry.json"), output.with_suffix(".needed.json")]
        if target == "windows":
            expected.append(case / "windows-diagnostics-imports.txt")
        result = run(relinker, case, command)
        check_plan(result, expected)
        assert "Converted executables: 1" in result.stdout and "Converted modules: 0" in result.stdout
        output.parent.mkdir(parents=True)
        for path in expected:
            path.write_bytes(b"existing output must survive")
        run(relinker, case, command)
        run(relinker, case, command, dry=False)
        magic = b"MZ" if target == "windows" else b"\xcf\xfa\xed\xfe" if target == "macos" else b"\x7fELF"
        assert output.read_bytes().startswith(magic)
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
        module_dir = paths["module"].parent
        module_dir.rmdir()
        module_dir.parent.rmdir()
        before_files = {path for path in case.rglob("*") if path.is_file()}
        result = run(relinker, case, command)
        check_plan(result, paths.values())
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
    run(relinker, root, ["--autorun", *command], code=1, error="--dry-run conflicts with --autorun")
    run(relinker, root, ["--verify-patches", root / "absent.toml", source], code=1,
        error="--dry-run cannot be used with --verify-patches")
    run(relinker, root, ["--skip-sce-module", source, source], code=2, error="overwrite the input")
    output.mkdir()
    run(relinker, root, command, code=2, error="Executable output is a directory")
    output.rmdir()
    source.write_bytes(b"not an executable")
    run(relinker, root, command, code=2, error="FAIL:")
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


def main():
    relinker = Path(sys.argv[1]).resolve()
    group = sys.argv[2]
    with tempfile.TemporaryDirectory(prefix=f"anyps5-dry-run-{group}-") as directory:
        globals()[f"check_{group}"](relinker, Path(directory))
    print(f"dry run {group}: passed")


if __name__ == "__main__":
    main()
