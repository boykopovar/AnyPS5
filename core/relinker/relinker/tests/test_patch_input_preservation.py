import os
from pathlib import Path
import subprocess
import sys
import tempfile

from test_game_patches import patch_site, patch_text
from test_guest_intel_trampolines import main_fixture
from test_secondary_output_collisions import prepare_secondary


def main():
    relinker = Path(sys.argv[1]).resolve()
    text = patch_text(*patch_site(main_fixture()), target="input.elf").encode()
    with tempfile.TemporaryDirectory(prefix="anyps5-patch-input-") as directory:
        work = Path(directory)
        for windows in (False, True):
            command, outputs, _ = prepare_secondary(work / f"control-{windows}", windows)
            config = outputs["executable"].parent / "relinker-patches.toml"
            config.write_bytes(text)
            result = subprocess.run([str(relinker), *command], cwd=config.parent,
                                    capture_output=True, text=True, timeout=20)
            assert result.returncode == 0 and config.read_bytes() == text, (result.stdout, result.stderr)
            outputs["manifest"] = config.parent / "output.patches.toml"
            for name in outputs:
                for alias in ("direct", "hardlink", "symlink"):
                    case = work / f"{windows}-{name}-{alias}"
                    command, paths, inputs = prepare_secondary(case, windows)
                    paths["manifest"] = case / "output.patches.toml"
                    target = paths[name]
                    config = target if alias == "direct" else case / "relinker-patches.toml"
                    config.write_bytes(text)
                    if alias == "hardlink":
                        os.link(config, target)
                    elif alias == "symlink":
                        try:
                            target.symlink_to(config)
                        except OSError as error:
                            if os.name != "nt" or error.winerror != 1314:
                                raise
                            print("Patch symbolic-link case skipped: Windows symlink privilege unavailable")
                            continue
                    for path in paths.values():
                        if path != target:
                            path.write_bytes(b"previous output")
                    original = {path: path.read_bytes() for path in paths.values()}
                    options = ["--patches", str(config)] if alias == "direct" else []
                    result = subprocess.run([str(relinker), *options, *command], cwd=case,
                                            capture_output=True, text=True, timeout=20)
                    assert result.returncode == 2 and "would overwrite the patch file" in result.stderr, (
                        windows, name, alias, result.stdout, result.stderr)
                    assert config.read_bytes() == text
                    for path, data in {**inputs, **original}.items():
                        assert path.read_bytes() == data, path
                    assert not list(case.rglob(".anyps5-output-identity-*"))

        case = work / "empty-config"
        command, outputs, inputs = prepare_secondary(case, False)
        config = outputs["registry"]
        config.write_text("format = 1\n")
        result = subprocess.run([str(relinker), "--patches", str(config), *command], cwd=case,
                                capture_output=True, text=True, timeout=20)
        assert result.returncode == 2 and "would overwrite the patch file" in result.stderr, (result.stdout, result.stderr)
        assert config.read_text() == "format = 1\n"
        assert not outputs["executable"].exists()
        for path, data in inputs.items():
            assert path.read_bytes() == data, path

        case = work / "disabled-config"
        command, _, _ = prepare_secondary(case, False)
        config = case / "relinker-patches.toml"
        config.write_bytes(text)
        result = subprocess.run([str(relinker), "--no-patches", *command[:-1], str(config)], cwd=case,
                                capture_output=True, text=True, timeout=20)
        assert result.returncode == 0 and config.read_bytes().startswith(b"\x7fELF"), (result.stdout, result.stderr)
    print("Patch input preservation tests passed")


if __name__ == "__main__":
    main()
