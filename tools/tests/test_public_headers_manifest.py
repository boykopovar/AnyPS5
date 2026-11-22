import json
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

SCRIPT = 'public_headers_manifest.py'
TOOLS = Path(__file__).resolve().parents[1]


class Tests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)

    def run_tool(self, *args):
        return subprocess.run([sys.executable, str(TOOLS / SCRIPT), *map(str, args)], capture_output=True,
                              encoding="utf-8", timeout=10, env={**os.environ, "PYTHONIOENCODING": "ascii"})

    def fixture(self):
        directory = self.root / "core/libs/prx/libExample"
        directory.mkdir(parents=True)
        (directory / "libExample.hpp").write_bytes(b'#include "Api.hpp"\n')
        (directory / "Api.hpp").write_bytes(b'#include <cstdint>\n')
        (self.root / "LICENSE").write_bytes(b"example license\n")
        return directory

    def assert_manifest_matches_recursive_package_bytes(self):
        import hashlib
        before = {str(p.relative_to(self.root)): (p.read_bytes(), p.stat().st_mtime_ns) for p in self.root.rglob("*") if p.is_file()}
        result = self.run_tool("--root", self.root)
        self.assertEqual(result.returncode, 0, result.stderr)
        report = json.loads(result.stdout)
        expected = []
        for path, (data, _) in before.items():
            path = Path(path).as_posix()
            destination = "LICENSE" if path == "LICENSE" else "include/anyps5/" + path.removeprefix("core/libs/")
            expected.append({"path": destination, "size": len(data), "sha256": hashlib.sha256(data).hexdigest()})
        self.assertEqual(report["headers"], len(expected) - 1)
        self.assertEqual(report["entries"], sorted(expected, key=lambda item: item["path"]))
        self.assertEqual(before, {str(p.relative_to(self.root)): (p.read_bytes(), p.stat().st_mtime_ns) for p in self.root.rglob("*") if p.is_file()})

    def test_manifest_matches_recursive_package_bytes(self):
        self.fixture()
        self.assert_manifest_matches_recursive_package_bytes()

    def test_bom_prefixed_umbrella_and_nested_header_keep_manifest_sizes_and_hashes(self):
        directory = self.fixture()
        nested = directory / "Nested.hpp"
        deep = directory / "Deep.hpp"
        nested.write_bytes(b'#include "Deep.hpp"\n')
        deep.write_bytes(b"struct Deep {};\n")
        (directory / "Api.hpp").write_bytes(b'#include "Nested.hpp"\n')
        for header in (directory / "libExample.hpp", nested):
            with self.subTest(bom_header=header.name):
                original = header.read_bytes()
                header.write_bytes(b"\xef\xbb\xbf" + original)
                try:
                    self.assert_manifest_matches_recursive_package_bytes()
                finally:
                    header.write_bytes(original)

    def test_manifest_matches_archives_for_symlink_alias(self):
        import hashlib
        import tarfile
        import zipfile
        sys.path.insert(0, str(TOOLS))
        self.addCleanup(sys.path.pop, 0)
        import package_headers
        directory = self.fixture()
        target = directory / "Target.hpp"
        target.write_bytes(b"struct Target {};\n")
        alias = directory / "Alias.hpp"
        try:
            alias.symlink_to(target.name)
        except OSError as error:
            self.skipTest(str(error))
        (directory / "Api.hpp").write_bytes(b'#include "Alias.hpp"\n')
        result = self.run_tool("--root", self.root)
        self.assertEqual(result.returncode, 0, result.stderr)
        report = json.loads(result.stdout)
        output = self.root / "output"
        package_headers.package(self.root, output, "v1")
        with zipfile.ZipFile(output / "prx-headers-v1.zip") as archive:
            files = {name: archive.read(name) for name in archive.namelist()}
        with tarfile.open(output / "prx-headers-v1.tar.gz") as archive:
            self.assertEqual({name: archive.extractfile(name).read() for name in archive.getnames()}, files)
        entries = [{"path": name, "size": len(data), "sha256": hashlib.sha256(data).hexdigest()}
                   for name, data in sorted(files.items())]
        self.assertEqual(report["entries"], entries)
        self.assertIn("include/anyps5/prx/libExample/Alias.hpp", files)

    def test_missing_dependency_and_license_fail_without_json(self):
        directory = self.fixture()
        (directory / "Api.hpp").unlink()
        self.assertEqual(self.run_tool("--root", self.root).returncode, 2)
        (directory / "Api.hpp").write_bytes(b"")
        (self.root / "LICENSE").write_bytes(b"")
        result = self.run_tool("--root", self.root)
        self.assertEqual(result.returncode, 2)
        self.assertEqual(result.stdout, "")

    def test_comments_raw_strings_and_digit_separators_do_not_create_false_includes(self):
        directory = self.fixture()
        (directory / "Api.hpp").write_bytes(
            b'#include <cstdint>\n'
            b'// #include "MissingLineComment.hpp"\n'
            b'/*\n#include "MissingBlockComment.hpp"\n*/\n'
            b'constexpr auto kRaw = R"delim(\n#include "MissingRaw.hpp"\n)delim";\n'
            b'constexpr uint64_t kNum = 1\'000\'000;\n'
            b'constexpr char kQuote = \'"\';\n'
        )
        self.assert_manifest_matches_recursive_package_bytes()

    def test_linked_header_directories_keep_the_package_manifest(self):
        self.fixture()
        expected = self.run_tool("--root", self.root)
        self.assertEqual(expected.returncode, 0, expected.stderr)
        for name in ("core/libs", "core"):
            with self.subTest(directory=name):
                link = self.root / name
                target = self.root / ("physical-" + link.name)
                link.rename(target)
                try:
                    link.symlink_to(target, target_is_directory=True)
                except OSError as error:
                    target.rename(link)
                    if getattr(error, "winerror", None) == 1314:
                        self.skipTest("needs symbolic-link creation privilege")
                    raise
                try:
                    result = self.run_tool("--root", self.root)
                    self.assertEqual(result.returncode, 0, result.stderr)
                    self.assertEqual(json.loads(result.stdout), json.loads(expected.stdout))
                finally:
                    link.unlink()
                    target.rename(link)


if __name__ == "__main__":
    unittest.main()
