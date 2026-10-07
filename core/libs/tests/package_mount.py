"""Mount a synthetic plaintext PS5 package as /app0 and check the guest file APIs against its contents."""

import os
from pathlib import Path
import subprocess
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "relinker" / "relinker" / "tests"))
from package_fixture import PAGE, build_package, random_bytes


def main():
    test = Path(sys.argv[1]).resolve()
    files = {
        "eboot.bin": b"\x7fELF" + random_bytes(5000, 3),
        "data/random.bin": random_bytes(0x100000 + 123, 1),
        "data/sparse.bin": bytes(2 * PAGE) + b"tail" * 25,
        "data/zero.bin": bytes(3000),
        "sce_sys/keystone": b"k" * 96,
    }
    content = {"param.json": b'{"titleId":"TEST00000"}', "trophy2/npbind.dat": b"\x01" * 32}
    with tempfile.TemporaryDirectory(prefix="anyps5-package-mount-") as directory:
        work = Path(directory)
        package = work / "spiel-ü" / "game.pkg"
        package.parent.mkdir()
        package_bytes = build_package(files, content, sparse_paths={"data/sparse.bin"}, zero_paths={"data/zero.bin"})
        package.write_bytes(package_bytes)
        expected = {**files, **{f"sce_sys/{name}": data for name, data in content.items()}}
        for relative, data in expected.items():
            target = work / "expected" / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(data)
        host = work / "app0" / "sce_module"
        host.mkdir(parents=True)
        (host / "provider.prx.guest.prx").write_bytes(b"guest")
        environment = dict(os.environ)
        environment.pop("ANYPS5_PACKAGE", None)
        environment.pop("ANYPS5_OODLE", None)

        result = subprocess.run([str(test)], cwd=work, env={**environment, "ANYPS5_PACKAGE": str(package)}, capture_output=True, text=True, timeout=120)
        assert result.returncode == 0, ("override", result.returncode, result.stdout, result.stderr)

        local = work / test.name
        local.write_bytes(test.read_bytes())
        for library in test.parent.glob("*.dll"):
            (work / library.name).write_bytes(library.read_bytes())
        sidecar = work / "anyps5-package.ini"

        def write_sidecar(size):
            lines = [f"package={package}", f"size={size}", "content_id=UP0000-TEST00000_00-0000000000000000", "oodle="]
            sidecar.write_bytes(("\r\n".join(lines) + "\r\n").encode("utf-8"))

        write_sidecar(len(package_bytes))
        result = subprocess.run([str(local)], cwd=work, env=environment, capture_output=True, text=True, timeout=120)
        assert result.returncode == 0, ("sidecar", result.returncode, result.stdout, result.stderr)
        write_sidecar(len(package_bytes) + 1)
        result = subprocess.run([str(local)], cwd=work, env=environment, capture_output=True, text=True, timeout=120)
        assert result.returncode != 0 and "relink the package" in result.stderr, ("stale sidecar", result.returncode, result.stdout, result.stderr)
    print("Package mount integration tests passed")


if __name__ == "__main__":
    main()
