import os
from pathlib import Path
import stat
import subprocess
import sys
import tempfile

from test_guest_intel_trampolines import PLAIN_SITE, guest_fixture, main_fixture
from test_linux_desktop_entry_output import png
from test_secondary_output_collisions import prepare_secondary


def check_executable_hardlinks(relinker, work):
    previous = b"previous output"
    for windows in (False, True):
        for output_name in ("executable", "secondary-one"):
            for input_name in ("input.elf", "sample.prx", "secondary-one.bin"):
                for readonly in ((False, True) if os.name == "nt" else (False,)):
                    case = work / f"hardlink-{windows}-{output_name}-{input_name}-{readonly}"
                    command, outputs, inputs = prepare_secondary(case, windows)
                    protected = next(path for path in inputs if path.name == input_name)
                    target = outputs[output_name]
                    os.link(protected, target)
                    for path in outputs.values():
                        if path != target:
                            path.write_bytes(previous)
                    if readonly:
                        protected.chmod(stat.S_IREAD)
                    mode = protected.stat().st_mode
                    links = protected.stat().st_nlink
                    try:
                        result = subprocess.run([str(relinker), *command], cwd=case,
                                                capture_output=True, text=True, timeout=20)
                        assert result.returncode == 2 and "output would overwrite" in result.stderr, (
                            windows, output_name, input_name, readonly, result.stdout, result.stderr)
                        for path, original in inputs.items():
                            assert path.read_bytes() == original, path
                        for path in outputs.values():
                            assert path.read_bytes() == (inputs[protected] if path == target else previous), path
                        assert protected.stat().st_mode == mode
                        assert protected.stat().st_nlink == links
                        assert os.path.samefile(protected, target)
                        assert not list(case.rglob(".anyps5-output-identity-*"))
                    finally:
                        if readonly:
                            protected.chmod(stat.S_IWRITE)

    for readonly in ((False, True) if os.name == "nt" else (False,)):
        case = work / f"distinct-input-hardlinks-{readonly}"
        command, outputs, inputs = prepare_secondary(case, True)
        links = case / "separate-links"
        links.mkdir()
        for index, path in enumerate(inputs):
            os.link(path, links / f"input-{index}")
        original = next(iter(inputs.values()))
        for name, path in outputs.items():
            path.write_bytes(original)
            os.link(path, links / name)
        if readonly:
            for path in inputs:
                path.chmod(stat.S_IREAD)
        try:
            result = subprocess.run([str(relinker), *command], cwd=case,
                                    capture_output=True, text=True, timeout=20)
            assert result.returncode == 0, (readonly, result.stdout, result.stderr)
            for name in ("executable", "secondary-one", "secondary-two"):
                assert outputs[name].read_bytes().startswith(b"MZ")
            for path, original in inputs.items():
                assert path.read_bytes() == original and path.stat().st_nlink == 2, path
            assert not list(case.rglob(".anyps5-output-identity-*"))
        finally:
            if readonly:
                for path in inputs:
                    path.chmod(stat.S_IWRITE)


def check_diagnostic_inputs(relinker, work):
    filename = "windows-diagnostics-imports.txt"
    for bundled in (False, True):
        for alias in ("direct", "hardlink", "symlink"):
            for registry in (False, True):
                case = work / f"diagnostics-{bundled}-{alias}-{registry}"
                modules = case / "sce_module"
                modules.mkdir(parents=True)
                diagnostics = (modules if bundled and alias == "direct" else case) / filename
                source = diagnostics if not bundled and alias == "direct" else case / "input.elf"
                original = bytes(main_fixture())
                source.write_bytes(original)
                module = diagnostics if bundled and alias == "direct" else modules / "sample.prx"
                module_original = bytes(guest_fixture(PLAIN_SITE))
                module.write_bytes(module_original)
                protected = module if bundled else source
                if alias == "hardlink":
                    os.link(protected, diagnostics)
                elif alias == "symlink":
                    try:
                        diagnostics.symlink_to(protected)
                    except OSError as error:
                        if os.name != "nt" or error.winerror != 1314:
                            raise
                        print("Diagnostic symbolic-link case skipped: Windows symlink privilege unavailable")
                        continue
                output = case / "output.exe"
                previous_output = b"previous executable"
                output.write_bytes(previous_output)
                registry_path = case / "output.registry.json"
                previous_registry = b"previous registry"
                registry_path.write_bytes(previous_registry)
                options = [] if bundled else ["--skip-sce-module"]
                if registry:
                    options.append("--registry")
                result = subprocess.run([str(relinker), "--windows", "--windows-diagnostics", *options,
                                         str(source), str(output)], cwd=diagnostics.parent,
                                        capture_output=True, text=True, timeout=20)
                owner = "an input module" if bundled else "the input executable"
                assert result.returncode == 2 and f"Windows import diagnostics output would overwrite {owner}" in result.stderr, (result.stdout, result.stderr)
                assert str(diagnostics) in result.stderr, result.stderr
                assert source.read_bytes() == original and module.read_bytes() == module_original
                assert diagnostics.read_bytes() == (module_original if bundled else original)
                assert output.read_bytes() == previous_output and registry_path.read_bytes() == previous_registry
                assert not (case / "app0").exists()

    for enabled in (False, True):
        for existing in (False, True):
            case = work / f"diagnostics-valid-{enabled}-{existing}"
            case.mkdir()
            diagnostics = case / filename
            source = case / "input.elf" if enabled else diagnostics
            original = bytes(main_fixture())
            source.write_bytes(original)
            if enabled and existing:
                diagnostics.write_bytes(b"previous diagnostics")
            output = case / "output.exe"
            options = ["--windows-diagnostics"] if enabled else []
            result = subprocess.run([str(relinker), "--windows", "--skip-sce-module", *options,
                                     str(source), str(output)], cwd=case,
                                    capture_output=True, text=True, timeout=20)
            assert result.returncode == 0 and output.read_bytes().startswith(b"MZ"), (result.stdout, result.stderr)
            assert source.read_bytes() == original
            assert diagnostics.read_bytes() == (b"" if enabled else original)


def check_registry_inputs(relinker, work, windows, needed=False):
    options = ["--windows"] if windows else []
    for module_registry in (False, True):
        for bundled in (False, True):
            for alias in ("direct", "hardlink", "symlink"):
                if bundled and alias == "direct":
                    continue
                case = work / f"registry-{windows}-{module_registry}-{bundled}-{alias}-{needed}"
                modules = case / "sce_module"
                modules.mkdir(parents=True)
                module = modules / "sample.prx"
                module_original = bytes(guest_fixture(PLAIN_SITE))
                module.write_bytes(module_original)
                output = case / ("output.exe" if windows else "output.elf")
                suffix = ".needed.json" if needed else ".registry.json"
                registries = [case / ("output" + suffix), case / ("output.sample.prx.guest.prx" + suffix)]
                registry = registries[int(module_registry)]
                source = registry if alias == "direct" else case / "input.elf"
                original = bytes(main_fixture())
                source.write_bytes(original)
                protected = module if bundled else source
                if alias == "hardlink":
                    os.link(protected, registry)
                elif alias == "symlink":
                    try:
                        registry.symlink_to(protected)
                    except OSError as error:
                        if os.name != "nt" or error.winerror != 1314:
                            raise
                        print("Registry symbolic-link case skipped: Windows symlink privilege unavailable")
                        continue
                previous_registry = b"previous registry"
                untouched = registries[int(not module_registry)]
                untouched.write_bytes(previous_registry)
                previous_output = b"previous executable"
                output.write_bytes(previous_output)
                result = subprocess.run([str(relinker), *options, "--registry", str(source), str(output)],
                                        capture_output=True, text=True, timeout=20)
                kind = "Needed libraries" if needed else "Registry"
                expected = f"{kind} output would overwrite an input module" if bundled else f"{kind} output would overwrite the input executable"
                assert result.returncode == 2 and expected in result.stderr, (result.returncode, result.stdout, result.stderr)
                assert source.read_bytes() == original and module.read_bytes() == module_original
                assert output.read_bytes() == previous_output and untouched.read_bytes() == previous_registry
                assert registry.read_bytes() == (module_original if bundled else original)
                assert not (case / "app0").exists()


def check_registry_after_failed_patch(relinker, work):
    case = work / "registry-failed-patch"
    (case / "sce_module").mkdir(parents=True)
    (case / "sce_module" / "sample.prx").write_bytes(guest_fixture(PLAIN_SITE))
    (case / "sce_sys").mkdir()
    (case / "sce_sys" / "icon0.png").write_bytes(b"\x89PNG\r\n\x1a\n" + bytes(40))
    source = case / "input.elf"
    source.write_bytes(main_fixture())
    output = case / "output.exe"
    previous_output = b"previous executable"
    output.write_bytes(previous_output)
    registries = [case / "output.registry.json", case / "output.sample.prx.guest.prx.registry.json",
                  case / "output.needed.json", case / "output.sample.prx.guest.prx.needed.json"]
    previous_registry = b"previous registry"
    for registry in registries:
        registry.write_bytes(previous_registry)
    result = subprocess.run([str(relinker), "--windows", "--registry", str(source), str(output)],
                            capture_output=True, text=True, timeout=20)
    assert result.returncode == 2 and "Invalid Windows icon PNG" in result.stderr, (result.stdout, result.stderr)
    assert output.read_bytes() == previous_output
    assert all(registry.read_bytes() == previous_registry for registry in registries)
    assert not (case / "app0").exists()


def check_output_directory(relinker, work):
    for windows in (False, True):
        for existing in (False, True):
            case = work / f"output-directory-{windows}-{existing}"
            (case / "sce_module").mkdir(parents=True)
            (case / "sce_module" / "sample.prx").write_bytes(guest_fixture(PLAIN_SITE))
            source = case / "input.elf"
            source.write_bytes(main_fixture())
            target = case / "output"
            if existing:
                target.mkdir()
            output = str(target) if existing else str(target) + os.sep
            options = ["--windows"] if windows else []
            result = subprocess.run([str(relinker), *options, str(source), output], cwd=case,
                                    capture_output=True, text=True, timeout=20)
            assert result.returncode == 2 and "Executable output is a directory" in result.stderr, (result.stdout, result.stderr)
            assert not (case / "app0").exists() and not (target / "app0").exists()
            assert target.exists() == existing


def check_desktop_inputs(relinker, work):
    for desktop in (False, True):
        for bundled in (False, True):
            for alias in ("direct", "hardlink", "symlink"):
                for registry in (False, True):
                    case = work / f"desktop-{desktop}-{bundled}-{alias}-{registry}"
                    modules = case / "sce_module"
                    modules.mkdir(parents=True)
                    sce_sys = case / "sce_sys"
                    sce_sys.mkdir()
                    image = png(1, 1)
                    (sce_sys / "icon0.png").write_bytes(image)
                    output = (modules if bundled and alias == "direct" else case) / "output.elf"
                    paths = [Path(f"{output}.png"), Path(f"{output}.desktop")]
                    target = paths[int(desktop)]
                    source = target if not bundled and alias == "direct" else case / "input.elf"
                    original = bytes(main_fixture())
                    source.write_bytes(original)
                    module = target if bundled and alias == "direct" else modules / "sample.prx"
                    module_original = bytes(guest_fixture(PLAIN_SITE))
                    module.write_bytes(module_original)
                    protected = module if bundled else source
                    if alias == "hardlink":
                        os.link(protected, target)
                    elif alias == "symlink":
                        try:
                            target.symlink_to(protected)
                        except OSError as error:
                            if os.name != "nt" or error.winerror != 1314:
                                raise
                            print("Desktop symbolic-link case skipped: Windows symlink privilege unavailable")
                            continue
                    previous_output = b"previous executable"
                    output.write_bytes(previous_output)
                    untouched = paths[int(not desktop)]
                    previous_auxiliary = b"previous auxiliary output"
                    untouched.write_bytes(previous_auxiliary)
                    registry_path = output.with_suffix(".registry.json")
                    previous_registry = b"previous registry"
                    registry_path.write_bytes(previous_registry)
                    options = ["--registry"] if registry else []
                    result = subprocess.run([str(relinker), *options, str(source), str(output)],
                                            capture_output=True, text=True, timeout=20)
                    kind = "Desktop entry" if desktop else "Icon"
                    owner = "an input module" if bundled else "the input executable"
                    assert result.returncode == 2 and f"{kind} output would overwrite {owner}" in result.stderr, (result.stdout, result.stderr)
                    assert str(target) in result.stderr, result.stderr
                    assert source.read_bytes() == original and module.read_bytes() == module_original
                    assert output.read_bytes() == previous_output and untouched.read_bytes() == previous_auxiliary
                    assert registry_path.read_bytes() == previous_registry
                    assert target.read_bytes() == (module_original if bundled else original)
                    assert (sce_sys / "icon0.png").read_bytes() == image
                    assert not (output.parent / "app0").exists()


def main():
    relinker = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="anyps5-input-preservation-") as directory:
        work = Path(directory)
        check_executable_hardlinks(relinker, work)
        check_diagnostic_inputs(relinker, work)
        check_desktop_inputs(relinker, work)
        check_registry_after_failed_patch(relinker, work)
        check_output_directory(relinker, work)
        for windows in (False, True):
            options = ["--windows"] if windows else []
            check_registry_inputs(relinker, work, windows)
            check_registry_inputs(relinker, work, windows, needed=True)
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
                if not windows:
                    (case / "sce_sys").mkdir()
                    image = png(1, 1)
                    (case / "sce_sys" / "icon0.png").write_bytes(image)
                    if existing:
                        Path(f"{output}.png").write_bytes(b"previous icon")
                        Path(f"{output}.desktop").write_bytes(b"previous desktop entry")
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
                if not windows:
                    assert Path(f"{output}.png").read_bytes() == image
                    assert Path(f"{output}.desktop").read_text().startswith("[Desktop Entry]\n")
    print("Input preservation tests passed")


if __name__ == "__main__":
    main()
