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
        package = work / "game.pkg"
        package.write_bytes(build_package(files, content, sparse_paths={"data/sparse.bin"}, zero_paths={"data/zero.bin"}))
        expected = {**files, **{f"sce_sys/{name}": data for name, data in content.items()}}
        for relative, data in expected.items():
            target = work / "expected" / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(data)
        host = work / "app0" / "sce_module"
        host.mkdir(parents=True)
        (host / "provider.prx.guest.prx").write_bytes(b"guest")
        environment = dict(os.environ, ANYPS5_PACKAGE=str(package))
        environment.pop("ANYPS5_OODLE", None)
        result = subprocess.run([str(test)], cwd=work, env=environment, capture_output=True, text=True, timeout=120)
        assert result.returncode == 0, (result.returncode, result.stdout, result.stderr)
    print("Package mount integration tests passed")


if __name__ == "__main__":
    main()
