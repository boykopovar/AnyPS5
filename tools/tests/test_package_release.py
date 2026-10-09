import sys
import tarfile
import tempfile
import unittest
import zipfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import package_release


class PackageReleaseTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.build = self.root / "build"
        self.output = self.root / "output"
        libraries = self.build / "core/libs/libs"
        libraries.mkdir(parents=True)
        for directory in Path("core/libs/prx").iterdir():
            if directory.is_dir():
                (libraries / f"{directory.name}.prx").write_bytes(b"native fixture")
        binary = self.build / "core/relinker"
        binary.mkdir(parents=True)
        (binary / "relinker.exe").write_bytes(b"relinker fixture")
        self.runtime = self.root / "compiler with spaces/bin"
        self.runtime.mkdir(parents=True)
        self.names = ("libgcc_s_seh-1.dll", "libstdc++-6.dll", "libwinpthread-1.dll")
        for name in self.names:
            (self.runtime / name).write_bytes(name.encode())

    def cache(self, kind="FILEPATH"):
        (self.build / "CMakeCache.txt").write_text(
            f"CMAKE_CXX_COMPILER:{kind}={self.runtime.as_posix()}/g++.exe\n", encoding="utf-8"
        )

    def test_windows_uses_configured_runtime_in_both_archives(self):
        for kind in ("FILEPATH", "STRING"):
            with self.subTest(kind=kind):
                self.cache(kind)
                package_release.package("windows", self.build, self.output, "vtest")
                with zipfile.ZipFile(self.output / "prx-windows-vtest.zip") as archive:
                    for name in self.names:
                        self.assertEqual(archive.read(f"libs/{name}"), name.encode())
                with tarfile.open(self.output / "prx-windows-vtest.tar.gz") as archive:
                    for name in self.names:
                        self.assertEqual(archive.extractfile(f"libs/{name}").read(), name.encode())
                self.assertEqual((self.output / "relinker-vtest.exe").read_bytes(), b"relinker fixture")

    def test_linux_needs_no_compiler_cache_or_windows_runtime(self):
        (self.build / "core/relinker/relinker").write_bytes(b"linux relinker fixture")
        package_release.package("linux", self.build, self.output, "vtest")
        with zipfile.ZipFile(self.output / "prx-linux-vtest.zip") as archive:
            self.assertTrue(all(name.endswith(".prx") for name in archive.namelist()))
        self.assertEqual((self.output / "relinker-vtest").read_bytes(), b"linux relinker fixture")

    def test_missing_compiler_record_rejected_before_output(self):
        (self.build / "CMakeCache.txt").write_text("CMAKE_BUILD_TYPE:STRING=Release\n", encoding="utf-8")
        with self.assertRaisesRegex(RuntimeError, "CMAKE_CXX_COMPILER"):
            package_release.package("windows", self.build, self.output, "vtest")
        self.assertFalse(self.output.exists())

    def test_missing_or_empty_runtime_rejected_before_output(self):
        self.cache()
        runtime = self.runtime / self.names[0]
        runtime.unlink()
        for empty in (False, True):
            with self.subTest(empty=empty):
                if empty:
                    runtime.touch()
                with self.assertRaisesRegex(RuntimeError, "Missing or empty release file"):
                    package_release.package("windows", self.build, self.output, "vtest")
                self.assertFalse(self.output.exists())


if __name__ == "__main__":
    unittest.main()
