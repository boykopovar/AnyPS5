import os
import stat
import sys
import tarfile
import tempfile
import unittest
import zipfile
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import package_headers


class PackageHeadersBasicArchiveTests(unittest.TestCase):
    def setUp(self):
        self.temp_dir = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp_dir.cleanup)
        self.root = Path(self.temp_dir.name)
        self.libs = self.root / "core/libs"
        self.lib_dir = self.libs / "prx/libSample"
        self.lib_dir.mkdir(parents=True)
        self.output = self.root / "output"
        self.license = self.root / "LICENSE"
        self.license.write_bytes(b"Sample License 1.0\n")
        self.umbrella = self.lib_dir / "libSample.hpp"
        self.umbrella.write_bytes(b'#include "Api.hpp"\n')
        self.api = self.lib_dir / "Api.hpp"
        self.api.write_bytes(b"#include <cstdint>\n")

    def test_basic_packaging_creates_zip_and_tar_archives(self):
        package_headers.package(self.root, self.output, "v1.0.0")
        zip_path = self.output / "prx-headers-v1.0.0.zip"
        tar_path = self.output / "prx-headers-v1.0.0.tar.gz"
        self.assertTrue(zip_path.is_file())
        self.assertTrue(tar_path.is_file())
        expected = {
            "LICENSE": self.license.read_bytes(),
            "include/anyps5/prx/libSample/libSample.hpp": self.umbrella.read_bytes(),
            "include/anyps5/prx/libSample/Api.hpp": self.api.read_bytes(),
        }
        with zipfile.ZipFile(zip_path) as archive:
            self.assertEqual(sorted(archive.namelist()), sorted(expected.keys()))
            for name, content in expected.items():
                self.assertEqual(archive.read(name), content)
        with tarfile.open(tar_path) as archive:
            self.assertEqual(sorted(archive.getnames()), sorted(expected.keys()))
            for name, content in expected.items():
                self.assertEqual(archive.extractfile(name).read(), content)

    def test_package_with_source_date_epoch(self):
        epoch = "1700000000"
        with patch.dict(os.environ, {"SOURCE_DATE_EPOCH": epoch}):
            package_headers.package(self.root, self.output, "v1.2.3")
        zip_path = self.output / "prx-headers-v1.2.3.zip"
        tar_path = self.output / "prx-headers-v1.2.3.tar.gz"
        first_zip_bytes = zip_path.read_bytes()
        with zipfile.ZipFile(zip_path) as archive:
            for info in archive.infolist():
                self.assertEqual(info.create_system, 3)
                self.assertEqual(info.external_attr >> 16, stat.S_IFREG | 0o644)
        with tarfile.open(tar_path) as archive:
            for info in archive.getmembers():
                self.assertEqual(info.mtime, int(epoch))
                self.assertEqual(info.mode, 0o644)
                self.assertEqual(info.uid, 0)
                self.assertEqual(info.gid, 0)
        second_output = self.root / "output-second"
        with patch.dict(os.environ, {"SOURCE_DATE_EPOCH": epoch}):
            package_headers.package(self.root, second_output, "v1.2.3")
            package_headers.package(self.root, self.output, "v1.2.3")
        self.assertEqual(first_zip_bytes, (second_output / "prx-headers-v1.2.3.zip").read_bytes())
        self.assertEqual(first_zip_bytes, zip_path.read_bytes())

    def test_invalid_version_tag_raises_value_error(self):
        for version in ("1.0.0", "v", "v1.", "v!bad", "alpha-v1"):
            with self.subTest(version=version):
                with self.assertRaises(ValueError):
                    package_headers.package(self.root, self.output, version)

    def test_missing_license_raises_runtime_error(self):
        self.license.unlink()
        with self.assertRaisesRegex(RuntimeError, "Missing project license"):
            package_headers.package(self.root, self.output, "v1.0")
        self.license.write_bytes(b"")
        with self.assertRaisesRegex(RuntimeError, "Missing project license"):
            package_headers.package(self.root, self.output, "v1.0")

    def test_output_destination_directory_conflict_raises_runtime_error(self):
        conflict = self.output / "prx-headers-v1.0.zip"
        conflict.mkdir(parents=True)
        with self.assertRaisesRegex(RuntimeError, "Release destination is"):
            package_headers.package(self.root, self.output, "v1.0")


class PackageHeadersFilteringTests(unittest.TestCase):
    def setUp(self):
        self.temp_dir = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp_dir.cleanup)
        self.root = Path(self.temp_dir.name)
        self.libs = self.root / "core/libs"
        self.lib_dir = self.libs / "prx/libSample"
        self.lib_dir.mkdir(parents=True)
        self.output = self.root / "output"
        self.license = self.root / "LICENSE"
        self.license.write_bytes(b"Sample License\n")
        self.umbrella = self.lib_dir / "libSample.hpp"
        self.umbrella.write_bytes(b'#include "Api.hpp"\n')
        self.api = self.lib_dir / "Api.hpp"
        self.api.write_bytes(b"#include <cstdint>\n")

    def test_internal_and_unreferenced_files_are_excluded(self):
        (self.lib_dir / "sample.cpp").write_bytes(b"int main() { return 0; }\n")
        (self.lib_dir / "InternalPrivate.hpp").write_bytes(b"struct Internal {};\n")
        (self.lib_dir / "README.md").write_bytes(b"# Documentation\n")
        (self.libs / "unreferenced.hpp").write_bytes(b"struct Orphan {};\n")
        hidden_dir = self.libs / "prx/.hidden"
        hidden_dir.mkdir(parents=True)
        (hidden_dir / "ignored.hpp").write_bytes(b"struct Hidden {};\n")
        package_headers.package(self.root, self.output, "v1.0.0")
        zip_path = self.output / "prx-headers-v1.0.0.zip"
        tar_path = self.output / "prx-headers-v1.0.0.tar.gz"
        expected_paths = {
            "LICENSE",
            "include/anyps5/prx/libSample/libSample.hpp",
            "include/anyps5/prx/libSample/Api.hpp",
        }
        with zipfile.ZipFile(zip_path) as archive:
            self.assertEqual(set(archive.namelist()), expected_paths)
        with tarfile.open(tar_path) as archive:
            self.assertEqual(set(archive.getnames()), expected_paths)

    def test_comments_and_raw_strings_are_filtered_from_include_scanning(self):
        self.umbrella.write_text(
            '#include "Api.hpp"\n'
            '// #include "MissingComment.hpp"\n'
            '/* #include "MissingBlockComment.hpp" */\n'
            'const char* s = R"(#include "MissingRaw.hpp")";\n',
            encoding="utf-8",
        )
        package_headers.package(self.root, self.output, "v1.0.0")
        with zipfile.ZipFile(self.output / "prx-headers-v1.0.0.zip") as archive:
            self.assertIn("include/anyps5/prx/libSample/Api.hpp", archive.namelist())
            self.assertNotIn("include/anyps5/prx/libSample/MissingComment.hpp", archive.namelist())
            self.assertNotIn("include/anyps5/prx/libSample/MissingBlockComment.hpp", archive.namelist())
            self.assertNotIn("include/anyps5/prx/libSample/MissingRaw.hpp", archive.namelist())

    def test_forbidden_include_paths_raise_runtime_error(self):
        for bad_include in ('"3rdparty/foo.hpp"', '"sub\\\\bad.hpp"', '"/abs/path.hpp"', '":bad.hpp"'):
            with self.subTest(include=bad_include):
                self.umbrella.write_text(f'#include {bad_include}\n', encoding="utf-8")
                with self.assertRaisesRegex(RuntimeError, "Forbidden public include"):
                    package_headers.package(self.root, self.output, "v1.0.0")

    def test_include_outside_core_libs_raises_runtime_error(self):
        outside = self.root / "outside.hpp"
        outside.write_bytes(b"struct Outside {};\n")
        self.umbrella.write_text('#include "../../../../outside.hpp"\n', encoding="utf-8")
        with self.assertRaisesRegex(RuntimeError, "Public header is outside core/libs"):
            package_headers.package(self.root, self.output, "v1.0.0")

    def test_sys_stat_is_a_host_header(self):
        self.api.write_bytes(b"#include <sys/stat.h>\n")
        self.assertEqual(set(package_headers.public_headers(self.root)), {self.api, self.umbrella})

    def test_unsupported_external_include_raises_runtime_error(self):
        self.api.write_text("#include <external_third_party.h>\n", encoding="utf-8")
        with self.assertRaisesRegex(RuntimeError, "Missing project or unsupported external include"):
            package_headers.package(self.root, self.output, "v1.0.0")

    def test_missing_referenced_header_raises_runtime_error(self):
        self.umbrella.write_text('#include "DoesNotExist.hpp"\n', encoding="utf-8")
        with self.assertRaisesRegex(RuntimeError, "Missing project or unsupported external include"):
            package_headers.package(self.root, self.output, "v1.0.0")

    def test_missing_umbrella_header_raises_runtime_error(self):
        self.umbrella.unlink()
        with self.assertRaisesRegex(RuntimeError, "Missing public header"):
            package_headers.package(self.root, self.output, "v1.0.0")

    def test_empty_public_library_directory_raises_runtime_error(self):
        for path in self.lib_dir.iterdir():
            path.unlink()
        self.lib_dir.rmdir()
        with self.assertRaisesRegex(RuntimeError, "No public library headers found"):
            package_headers.package(self.root, self.output, "v1.0.0")


class PackageHeadersSymlinkTests(unittest.TestCase):
    def setUp(self):
        self.temp_dir = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp_dir.cleanup)
        self.root = Path(self.temp_dir.name)
        self.libs = self.root / "core/libs"
        self.lib_dir = self.libs / "prx/libSample"
        self.lib_dir.mkdir(parents=True)
        self.output = self.root / "output"
        self.license = self.root / "LICENSE"
        self.license.write_bytes(b"Sample License\n")
        self.umbrella = self.lib_dir / "libSample.hpp"
        self.umbrella.write_bytes(b'#include "Api.hpp"\n')
        self.api = self.lib_dir / "Api.hpp"
        self.api.write_bytes(b"#include <cstdint>\n")

    def test_symlink_alias_is_packaged_with_target_content(self):
        target = self.lib_dir / "Target.hpp"
        target.write_bytes(b"struct TargetHeader {};\n")
        alias = self.lib_dir / "Alias.hpp"
        try:
            alias.symlink_to(target.name)
        except OSError as error:
            self.skipTest(str(error))
        self.api.write_text('#include "Alias.hpp"\n', encoding="utf-8")
        package_headers.package(self.root, self.output, "v1.0.0")
        zip_path = self.output / "prx-headers-v1.0.0.zip"
        tar_path = self.output / "prx-headers-v1.0.0.tar.gz"
        alias_arcname = "include/anyps5/prx/libSample/Alias.hpp"
        with zipfile.ZipFile(zip_path) as archive:
            self.assertIn(alias_arcname, archive.namelist())
            self.assertEqual(archive.read(alias_arcname), b"struct TargetHeader {};\n")
        with tarfile.open(tar_path) as archive:
            self.assertIn(alias_arcname, archive.getnames())
            self.assertEqual(archive.extractfile(alias_arcname).read(), b"struct TargetHeader {};\n")

    def test_symlink_cycle_detected_raises_runtime_error(self):
        cycle_a = self.lib_dir / "cycle_a.hpp"
        cycle_b = self.lib_dir / "cycle_b.hpp"
        try:
            cycle_a.symlink_to(cycle_b.name)
            cycle_b.symlink_to(cycle_a.name)
        except OSError as error:
            self.skipTest(str(error))
        with self.assertRaisesRegex(RuntimeError, "Symbolic link loop"):
            package_headers.resolved_path(cycle_a)

    def test_symlink_loop_detected_via_mock(self):
        with patch.object(Path, "is_symlink", return_value=True):
            with self.assertRaisesRegex(RuntimeError, "Symbolic link loop"):
                package_headers.resolved_path(self.lib_dir / "fake_loop.hpp")


if __name__ == "__main__":
    unittest.main()
