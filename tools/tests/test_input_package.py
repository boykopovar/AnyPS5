import importlib.util
import os
import tarfile
import tempfile
import unittest
import zipfile
from pathlib import Path
from unittest.mock import patch

spec = importlib.util.spec_from_file_location("package_release", Path(__file__).parents[1] / "package_release.py")
packager = importlib.util.module_from_spec(spec)
spec.loader.exec_module(packager)


class InputPackageTests(unittest.TestCase):
    def test_packages_include_native_input_tool(self):
        original = Path.cwd()
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            try:
                os.chdir(root)
                (root / "core/libs/prx/libTest").mkdir(parents=True)
                build = root / "build"
                libraries = build / "core/libs/libs"
                libraries.mkdir(parents=True)
                for name in ("libTest.prx", "libcohtml.Prospero.prx"):
                    (libraries / name).write_bytes(b"library")
                runtime = root / "runtime"
                runtime.mkdir()
                for name in ("libgcc_s_seh-1.dll", "libstdc++-6.dll", "libwinpthread-1.dll"):
                    (runtime / name).write_bytes(b"runtime")
                for platform in ("linux", "windows"):
                    suffix = ".exe" if platform == "windows" else ""
                    binary = build / "core/input" / ("anyps5-input-config" + suffix)
                    binary.parent.mkdir(parents=True, exist_ok=True)
                    relinker = build / "core/relinker" / ("relinker" + suffix)
                    relinker.parent.mkdir(parents=True, exist_ok=True)
                    relinker.write_bytes(b"relinker")
                    binary.write_bytes(b"input editor")
                    binary.chmod(0o755)
                    output = root / platform
                    def release_path(value):
                        return runtime if value == "C:/winlibs/mingw64/bin" else Path(value)
                    with patch.object(packager, "Path", side_effect=release_path):
                        packager.package(platform, build, output, "v1-test")
                    with zipfile.ZipFile(output / f"prx-{platform}-v1-test.zip") as archive:
                        self.assertEqual(archive.read("anyps5-input-config" + suffix), b"input editor")
                        self.assertEqual(archive.read("libs/libTest.prx"), b"library")
                    with tarfile.open(output / f"prx-{platform}-v1-test.tar.gz") as archive:
                        self.assertEqual(archive.extractfile("anyps5-input-config" + suffix).read(), b"input editor")
                    self.assertEqual((output / ("anyps5-input-config-v1-test" + suffix)).read_bytes(), b"input editor")
                    binary.unlink()
                    with patch.object(packager, "Path", side_effect=release_path):
                        with self.assertRaises(RuntimeError):
                            packager.package(platform, build, root / (platform + "-missing"), "v2-test")
            finally:
                os.chdir(original)


if __name__ == "__main__":
    unittest.main()
