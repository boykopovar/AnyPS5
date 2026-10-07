"""Relink executables read directly from a plaintext PS5 package without extracting it."""

from pathlib import Path
import os
import subprocess
import sys
import tempfile

from package_fixture import PAGE, build_package, fake_self, random_bytes
from test_guest_intel_trampolines import main_fixture
from test_guest_module_directories import module_with_symbol


def main():
    relinker = Path(sys.argv[1]).resolve()
    files = {
        "eboot.bin": fake_self(main_fixture()),
        "sce_module/provider.prx": fake_self(module_with_symbol(True)),
        "sce_sys/keystone": b"k" * 96,
        "data/random.bin": random_bytes(0x100000 + 123, 1),
        "data/sparse.bin": bytes(2 * PAGE) + b"tail" * 25,
        "data/zero.bin": bytes(3000),
    }
    content = {"param.json": b'{"titleId":"TEST00000"}', "trophy2/npbind.dat": b"\x01" * 32}
    package = build_package(files, content, sparse_paths={"data/sparse.bin"}, zero_paths={"data/zero.bin"})

    with tempfile.TemporaryDirectory(prefix="anyps5-package-") as directory:
        work = Path(directory)

        def relink(name, package_bytes, *options, windows=False):
            case = work / name
            (case / "out").mkdir(parents=True)
            source = case / "game.pkg"
            source.write_bytes(package_bytes)
            output = case / "out" / ("app.exe" if windows else "app.elf")
            result = subprocess.run([str(relinker), *(["--windows"] if windows else []), *options, str(source), str(output)],
                                    capture_output=True, text=True, timeout=60)
            return result, output, source

        for windows in (False, True):
            result, output, source = relink(f"converted-{windows}", package, windows=windows)
            assert result.returncode == 0, (result.stdout, result.stderr)
            assert output.read_bytes().startswith(b"MZ" if windows else b"\x7fELF")
            assert "eboot.bin and 1 guest modules read from the package" in result.stdout, result.stdout
            app0 = output.parent / "app0"
            written = sorted(path.relative_to(app0).as_posix() for path in app0.rglob("*") if path.is_file())
            assert written == ["sce_module/provider.prx.guest.prx"], written
            sidecar = (output.parent / "anyps5-package.ini").read_text()
            assert sidecar == f"package={os.path.abspath(source)}\nsize={len(package)}\ncontent_id=UP0000-TEST00000_00-0000000000000000\noodle=\n", sidecar

        plain = work / "plain.elf"
        plain.write_bytes(main_fixture())
        result = subprocess.run([str(relinker), "--skip-sce-module", str(plain), str(output)], capture_output=True, text=True, timeout=60)
        assert result.returncode == 0 and not (output.parent / "anyps5-package.ini").exists(), (result.stdout, result.stderr)
        result = subprocess.run([str(relinker), "--oodle", "oo2core.dll", str(plain), str(work / "plain.out")], capture_output=True, text=True, timeout=60)
        assert result.returncode == 1 and "--oodle requires a PS5 .pkg input" in result.stderr, result

        kraken = build_package(files, content, kraken_path="sce_module/provider.prx")
        failures = [
            ("kraken", kraken, [], "an Oodle library (oo2core) path is required"),
            ("missing-oodle", kraken, ["--oodle", str(work / "missing-oo2core.dll")], "Cannot load Oodle library"),
            ("encrypted", build_package(files, content, marker=False), [], "Encrypted PS5 packages are not supported"),
            ("unsafe-name", build_package({**files, "data/a\\b": b"x"}, content), [], "Invalid inner package entry name"),
        ]
        for name, package_bytes, options, error in failures:
            result, output, _ = relink(name, package_bytes, *options)
            assert result.returncode == 2 and error in result.stderr and not output.exists(), (name, result.stdout, result.stderr)
    print("Package input tests passed")


if __name__ == "__main__":
    main()
