import os
import json
import ctypes
import stat
import subprocess
import sys
import tarfile
import tempfile
import unittest
import zipfile
from contextlib import contextmanager
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import package_release
import prepare_release_notes


@contextmanager
def working_directory(path):
    previous = Path.cwd()
    os.chdir(path)
    try:
        yield
    finally:
        os.chdir(previous)


class ReleasePackageTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.source = self.root / "source"
        self.build = self.root / "build"
        self.output = self.root / "output"
        (self.source / "core/libs/prx/libc").mkdir(parents=True)
        (self.source / "core/libs/prx/libc/Export.cpp").write_text("", encoding="utf-8")
        libraries = self.build / "core/libs/libs"
        libraries.mkdir(parents=True)
        self.library = libraries / "libc.prx"
        self.library.write_bytes(b"library bytes")
        binary = self.build / "core/relinker/relinker"
        binary.parent.mkdir(parents=True)
        binary.write_bytes(b"relinker bytes")
        self.addCleanup(patch.stopall)
        patch.object(package_release, "ROOT", self.source, create=True).start()

    def test_windows_runtime_follows_the_configured_compiler_directory(self):
        runtime = self.root / "custom toolchain" / "bin"
        runtime.mkdir(parents=True)
        names = ("libgcc_s_seh-1.dll", "libstdc++-6.dll", "libwinpthread-1.dll")
        for name in names:
            (runtime / name).write_bytes(name.encode())
        compiler = runtime / "g++.exe"
        (self.build / "CMakeCache.txt").write_text("CMAKE_CXX_COMPILER:STRING=" + compiler.as_posix() + "\n", encoding="utf-8")
        (self.build / "core/relinker/relinker.exe").write_bytes(b"Windows executable")
        package_release.package("windows", self.build, self.output, "v1")
        with zipfile.ZipFile(self.output / "prx-windows-v1.zip") as archive:
            self.assertEqual(set(archive.namelist()), {"libs/libc.prx", *("libs/" + name for name in names)})
            for name in names:
                self.assertEqual(archive.read("libs/" + name), name.encode())
        with tarfile.open(self.output / "prx-windows-v1.tar.gz") as archive:
            for name in names:
                self.assertEqual(archive.extractfile("libs/" + name).read(), name.encode())
        self.assertEqual((self.output / "relinker-v1.exe").read_bytes(), b"Windows executable")

    def test_windows_package_without_configured_compiler_preserves_existing_output(self):
        (self.build / "CMakeCache.txt").write_text("CMAKE_CXX_COMPILER:FILEPATH=compiler-NOTFOUND\n", encoding="utf-8")
        (self.build / "core/relinker/relinker.exe").write_bytes(b"Windows executable")
        self.output.mkdir()
        previous = self.output / "previous.asset"
        previous.write_bytes(b"existing output")
        with self.assertRaisesRegex(RuntimeError, "CMAKE_CXX_COMPILER"):
            package_release.package("windows", self.build, self.output, "v1")
        self.assertEqual(list(self.output.iterdir()), [previous])
        self.assertEqual(previous.read_bytes(), b"existing output")

    def test_tracy_packages_include_only_the_target_platform_client(self):
        directory = self.build / "core/libs/libs"
        for platform, name in (("linux", "libTracyClient.so.0.14.1"), ("windows", "libTracyClient.dll")):
            with self.subTest(platform=platform):
                client = directory / name
                client.write_bytes(platform.encode() + b" Tracy client")
                cache = "APS5_ENABLE_TRACY:BOOL=ON\n"
                expected = {"libs/libc.prx", "libs/" + name}
                if platform == "windows":
                    runtime = self.root / "toolchain"
                    runtime.mkdir()
                    for runtime_name in ("libgcc_s_seh-1.dll", "libstdc++-6.dll", "libwinpthread-1.dll"):
                        (runtime / runtime_name).write_bytes(runtime_name.encode())
                        expected.add("libs/" + runtime_name)
                    cache += "CMAKE_CXX_COMPILER:FILEPATH=" + (runtime / "g++.exe").as_posix() + "\n"
                    (self.build / "core/relinker/relinker.exe").write_bytes(b"Windows executable")
                (self.build / "CMakeCache.txt").write_text(cache, encoding="utf-8")
                package_release.package(platform, self.build, self.output, "v1")
                with zipfile.ZipFile(self.output / f"prx-{platform}-v1.zip") as archive:
                    self.assertEqual(set(archive.namelist()), expected)
                    self.assertEqual(archive.read("libs/" + name), client.read_bytes())
                with tarfile.open(self.output / f"prx-{platform}-v1.tar.gz") as archive:
                    self.assertEqual(set(archive.getnames()), expected)
                    self.assertEqual(archive.extractfile("libs/" + name).read(), client.read_bytes())

    def test_disabled_tracy_does_not_publish_stale_clients(self):
        for name in ("libTracyClient.dll", "libTracyClient.so.0.14.1"):
            (self.library.parent / name).write_bytes(b"old client")
        for index, value in enumerate(("OFF", "0", "NO", "FALSE", "N", "IGNORE", "NOTFOUND", "TRACY-NOTFOUND", "")):
            with self.subTest(value=value):
                (self.build / "CMakeCache.txt").write_text(f"APS5_ENABLE_TRACY:BOOL={value}\n", encoding="utf-8")
                output = self.output / str(index)
                package_release.package("linux", self.build, output, "v1")
                with zipfile.ZipFile(output / "prx-linux-v1.zip") as archive:
                    self.assertEqual(archive.namelist(), ["libs/libc.prx"])

    def test_missing_or_ambiguous_tracy_client_preserves_existing_assets(self):
        (self.build / "CMakeCache.txt").write_text("APS5_ENABLE_TRACY:BOOL=ON\n", encoding="utf-8")
        previous = {"prx-linux-v1.zip": b"old zip", "prx-linux-v1.tar.gz": b"old tar", "relinker-v1": b"old binary"}
        self.output.mkdir()
        for name, data in previous.items():
            (self.output / name).write_bytes(data)
        for clients in ((), ("libTracyClient.so.0.13.1", "libTracyClient.so.0.14.1")):
            with self.subTest(clients=clients):
                for name in clients:
                    (self.library.parent / name).write_bytes(b"client bytes")
                with self.assertRaisesRegex(RuntimeError, "Tracy"):
                    package_release.package("linux", self.build, self.output, "v1")
                self.assertEqual({file.name: file.read_bytes() for file in self.output.iterdir()}, previous)

    def test_source_discovery_does_not_depend_on_working_directory(self):
        with working_directory(self.root):
            package_release.package("linux", self.build, self.output, "v1.0")
        self.assertEqual((self.output / "relinker-v1.0").read_bytes(), b"relinker bytes")
        with zipfile.ZipFile(self.output / "prx-linux-v1.0.zip") as archive:
            self.assertEqual(archive.read("libs/libc.prx"), b"library bytes")

    def test_hidden_cache_directory_is_not_a_required_library(self):
        (self.source / "core/libs/prx/.cache").mkdir()
        with working_directory(self.source):
            package_release.package("linux", self.build, self.output, "v1.0")
        with zipfile.ZipFile(self.output / "prx-linux-v1.0.zip") as archive:
            self.assertEqual(archive.namelist(), ["libs/libc.prx"])

    def test_removed_libraries_are_not_packaged_from_a_reused_build(self):
        stale = self.library.with_name("libRemoved.prx")
        stale.write_bytes(b"obsolete library")
        package_release.package("linux", self.build, self.output, "v1.0")
        with zipfile.ZipFile(self.output / "prx-linux-v1.0.zip") as archive:
            self.assertEqual(archive.namelist(), ["libs/libc.prx"])
            self.assertEqual(archive.read("libs/libc.prx"), b"library bytes")
        with tarfile.open(self.output / "prx-linux-v1.0.tar.gz") as archive:
            self.assertEqual(archive.getnames(), ["libs/libc.prx"])
            self.assertEqual(archive.extractfile("libs/libc.prx").read(), b"library bytes")
        self.assertEqual(stale.read_bytes(), b"obsolete library")

    def test_tar_and_zip_include_the_same_symlink_contents(self):
        target = self.library.with_name("libc.target")
        self.library.rename(target)
        try:
            self.library.symlink_to(target.name)
        except (OSError, NotImplementedError) as error:
            self.skipTest(str(error))
        with working_directory(self.source):
            package_release.package("linux", self.build, self.output, "v1.0")
        with tarfile.open(self.output / "prx-linux-v1.0.tar.gz") as archive:
            member = archive.getmember("libs/libc.prx")
            self.assertTrue(member.isfile())
            self.assertEqual(archive.extractfile(member).read(), b"library bytes")
        with zipfile.ZipFile(self.output / "prx-linux-v1.0.zip") as archive:
            self.assertEqual(archive.read("libs/libc.prx"), b"library bytes")

    def test_missing_real_library_still_fails(self):
        self.library.unlink()
        with working_directory(self.source), self.assertRaisesRegex(RuntimeError, "Missing patched libraries"):
            package_release.package("linux", self.build, self.output, "v1.0")

    def test_directory_destination_preserves_every_existing_asset(self):
        names = ["prx-linux-v1.zip", "prx-linux-v1.tar.gz", "relinker-v1"]
        for blocked in names:
            with self.subTest(blocked=blocked), tempfile.TemporaryDirectory() as tmp:
                output = Path(tmp)
                for name in names:
                    destination = output / name
                    if name == blocked:
                        destination.mkdir()
                        (destination / "keep.txt").write_bytes(b"directory content")
                    else:
                        destination.write_bytes(b"old " + name.encode())
                (output / "unrelated.txt").write_bytes(b"unrelated")
                with self.assertRaises((OSError, RuntimeError)):
                    package_release.package("linux", self.build, output, "v1")
                self.assertEqual(sorted(path.name for path in output.iterdir()), sorted(names + ["unrelated.txt"]))
                for name in names:
                    destination = output / name
                    if name == blocked:
                        self.assertEqual((destination / "keep.txt").read_bytes(), b"directory content")
                    else:
                        self.assertEqual(destination.read_bytes(), b"old " + name.encode())
                self.assertEqual((output / "unrelated.txt").read_bytes(), b"unrelated")

    @unittest.skipUnless(os.name == "nt", "Windows read-only replacement semantics")
    def test_readonly_destinations_preserve_all_existing_assets(self):
        names = ["prx-linux-v1.zip", "prx-linux-v1.tar.gz", "relinker-v1"]
        for index, blocked in enumerate(names):
            with self.subTest(blocked=blocked):
                output = self.output / str(index)
                output.mkdir(parents=True)
                previous = {name: b"old " + name.encode() for name in names}
                previous["unrelated.txt"] = b"unrelated"
                for name, content in previous.items():
                    (output / name).write_bytes(content)
                destination = output / blocked
                destination.chmod(stat.S_IREAD)
                try:
                    with self.assertRaises((OSError, RuntimeError)) as error:
                        package_release.package("linux", self.build, output, "v1")
                    self.assertEqual({path.name: path.read_bytes() for path in output.iterdir()}, previous)
                    self.assertIn("read-only", str(error.exception))
                    self.assertFalse(destination.stat().st_mode & stat.S_IWRITE)
                finally:
                    destination.chmod(stat.S_IREAD | stat.S_IWRITE)

    @unittest.skipIf(os.name == "nt", "POSIX allows replacement in a writable directory")
    def test_posix_readonly_destination_can_be_replaced(self):
        self.output.mkdir()
        destination = self.output / "prx-linux-v1.zip"
        destination.write_bytes(b"old archive")
        destination.chmod(stat.S_IREAD)
        try:
            package_release.package("linux", self.build, self.output, "v1")
            with zipfile.ZipFile(destination) as archive:
                self.assertEqual(archive.read("libs/libc.prx"), b"library bytes")
        finally:
            destination.chmod(stat.S_IREAD | stat.S_IWRITE)

    @unittest.skipUnless(os.name == "nt", "Windows directory junction replacement semantics")
    def test_dangling_directory_junction_preserves_all_existing_assets(self):
        names = ["prx-linux-v1.zip", "prx-linux-v1.tar.gz", "relinker-v1"]
        for index, blocked in enumerate(names):
            with self.subTest(blocked=blocked):
                output = self.output / str(index)
                output.mkdir(parents=True)
                previous = {name: b"old " + name.encode() for name in names if name != blocked}
                previous["unrelated.txt"] = b"unrelated"
                for name, content in previous.items():
                    (output / name).write_bytes(content)
                target = self.root / f"target-{index}"
                target.mkdir()
                destination = output / blocked
                subprocess.run(["cmd", "/c", "mklink", "/J", str(destination), str(target)],
                               check=True, capture_output=True)
                self.addCleanup(destination.rmdir)
                target.rmdir()
                self.assertFalse(destination.exists())
                self.assertTrue(destination.lstat().st_file_attributes & stat.FILE_ATTRIBUTE_DIRECTORY)
                with self.assertRaises((OSError, RuntimeError)) as error:
                    package_release.package("linux", self.build, output, "v1")
                self.assertEqual({path.name: path.read_bytes() for path in output.iterdir() if path != destination}, previous)
                self.assertTrue(destination.lstat().st_file_attributes & stat.FILE_ATTRIBUTE_DIRECTORY)
                self.assertFalse(target.exists())
                self.assertIn("not a file", str(error.exception))

    def file_link(self, link, target):
        try:
            link.symlink_to(target)
        except OSError as error:
            self.skipTest(f"File symlinks unavailable: {error}")

    def link_attributes(self, link, attributes):
        setter = ctypes.WinDLL("kernel32", use_last_error=True).SetFileAttributesW
        setter.argtypes = [ctypes.c_wchar_p, ctypes.c_uint32]
        setter.restype = ctypes.c_int
        if not setter(str(link), attributes):
            raise ctypes.WinError(ctypes.get_last_error())

    @unittest.skipUnless(os.name == "nt", "Windows read-only symlink semantics")
    def test_readonly_dangling_link_preserves_existing_assets(self):
        self.output.mkdir()
        tar = self.output / "prx-linux-v1.tar.gz"
        binary = self.output / "relinker-v1"
        tar.write_bytes(b"old tar")
        binary.write_bytes(b"old executable")
        link = self.output / "prx-linux-v1.zip"
        self.file_link(link, "missing.zip")
        self.link_attributes(link, stat.FILE_ATTRIBUTE_READONLY)
        try:
            self.assertFalse(link.exists())
            with self.assertRaisesRegex(RuntimeError, "read-only"):
                package_release.package("linux", self.build, self.output, "v1")
            self.assertEqual(tar.read_bytes(), b"old tar")
            self.assertEqual(binary.read_bytes(), b"old executable")
            self.assertEqual(link.readlink(), Path("missing.zip"))
            self.assertTrue(link.lstat().st_file_attributes & stat.FILE_ATTRIBUTE_READONLY)
            self.assertEqual({path.name for path in self.output.iterdir()}, {tar.name, binary.name, link.name})
        finally:
            self.link_attributes(link, stat.FILE_ATTRIBUTE_NORMAL)

    @unittest.skipUnless(os.name == "nt", "Windows link and target attributes are distinct")
    def test_writable_link_to_readonly_target_can_be_replaced(self):
        self.output.mkdir()
        target = self.root / "original.zip"
        target.write_bytes(b"original target")
        link = self.output / "prx-linux-v1.zip"
        self.file_link(link, target)
        target.chmod(stat.S_IREAD)
        try:
            self.assertFalse(link.lstat().st_file_attributes & stat.FILE_ATTRIBUTE_READONLY)
            package_release.package("linux", self.build, self.output, "v1")
            self.assertFalse(link.is_symlink())
            self.assertEqual(target.read_bytes(), b"original target")
            self.assertFalse(target.stat().st_mode & stat.S_IWRITE)
            with zipfile.ZipFile(link) as archive:
                self.assertEqual(archive.read("libs/libc.prx"), b"library bytes")
        finally:
            target.chmod(stat.S_IREAD | stat.S_IWRITE)

    def test_invalid_tag_creates_no_assets(self):
        with self.assertRaises(ValueError):
            package_release.package("linux", self.build, self.output, "../v1.0")
        self.assertFalse(self.output.exists())



class DocumentationAssetsTests(unittest.TestCase):
    def test_unique_nested_documents_are_flattened(self):
        with tempfile.TemporaryDirectory() as tmp:
            source = Path(tmp) / "source"
            (source / "nested").mkdir(parents=True)
            (source / "A.md").write_text("A", encoding="utf-8")
            (source / "nested/B.md").write_text("B", encoding="utf-8")
            output = Path(tmp) / "assets"
            package_release.collect_docs(source, output)
            self.assertEqual({file.name: file.read_text(encoding="utf-8") for file in output.iterdir()}, {"A.md": "A", "B.md": "B"})

    def test_existing_case_collision_preserves_assets(self):
        with tempfile.TemporaryDirectory() as tmp:
            source = Path(tmp) / "source"
            source.mkdir()
            (source / "guide.md").write_text("new guide", encoding="utf-8")
            output = Path(tmp) / "assets"
            output.mkdir()
            (output / "Guide.md").write_text("old guide", encoding="utf-8")
            with self.assertRaisesRegex(RuntimeError, "Duplicate release asset"):
                package_release.collect_docs(source, output)
            self.assertEqual([file.name for file in output.iterdir()], ["Guide.md"])
            self.assertEqual((output / "Guide.md").read_text(encoding="utf-8"), "old guide")

    def test_markdown_directory_is_ignored(self):
        with tempfile.TemporaryDirectory() as tmp:
            source = Path(tmp) / "source"
            source.mkdir()
            (source / "A.md").write_text("A", encoding="utf-8")
            (source / "Z.md").mkdir()
            output = Path(tmp) / "assets"
            package_release.collect_docs(source, output)
            self.assertEqual([file.name for file in output.iterdir()], ["A.md"])
            self.assertEqual((output / "A.md").read_text(encoding="utf-8"), "A")

    def test_case_colliding_assets_fail_before_copying(self):
        with tempfile.TemporaryDirectory() as tmp:
            source = Path(tmp) / "source"
            (source / "one").mkdir(parents=True)
            (source / "two").mkdir()
            (source / "one/Guide.md").write_text("one", encoding="utf-8")
            (source / "two/guide.md").write_text("two", encoding="utf-8")
            output = Path(tmp) / "assets"
            with self.assertRaisesRegex(RuntimeError, "Duplicate release asset"):
                package_release.collect_docs(source, output)
            self.assertFalse(output.exists())

    def test_dangling_asset_symlink_is_not_followed(self):
        with tempfile.TemporaryDirectory() as tmp:
            source = Path(tmp) / "source"
            source.mkdir()
            (source / "Guide.md").write_text("guide", encoding="utf-8")
            output = Path(tmp) / "assets"
            output.mkdir()
            target = Path(tmp) / "outside.md"
            try:
                (output / "Guide.md").symlink_to(target)
            except (OSError, NotImplementedError) as error:
                self.skipTest(str(error))
            with self.assertRaisesRegex(RuntimeError, "Duplicate release asset"):
                package_release.collect_docs(source, output)
            self.assertFalse(target.exists())
            self.assertTrue((output / "Guide.md").is_symlink())


class ReleaseAssetsTests(unittest.TestCase):
    def test_failed_tar_creation_preserves_existing_release_assets(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            source = root / "source"
            (source / "core/libs/prx/libc").mkdir(parents=True)
            build = root / "build"
            (build / "core/libs/libs").mkdir(parents=True)
            (build / "core/libs/libs/libc.prx").write_bytes(b"new library")
            (build / "core/relinker").mkdir(parents=True)
            (build / "core/relinker/relinker").write_bytes(b"new executable")
            output = root / "assets"
            output.mkdir()
            names = ["prx-linux-v1.zip", "prx-linux-v1.tar.gz", "relinker-v1"]
            for name in names:
                (output / name).write_bytes(b"old " + name.encode())
            with working_directory(source), patch.object(package_release, "ROOT", source), \
                    patch.object(package_release.tarfile, "open", side_effect=OSError("disk full")), \
                    self.assertRaisesRegex(OSError, "disk full"):
                package_release.package("linux", build, output, "v1")
            self.assertEqual(sorted(file.name for file in output.iterdir()), sorted(names))
            for name in names:
                self.assertEqual((output / name).read_bytes(), b"old " + name.encode())


class ReleaseNoteDestinationsTests(unittest.TestCase):
    def test_directory_body_preserves_existing_full_notes(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source, body, full = root / "notes.json", root / "body", root / "full.md"
            source.write_text(json.dumps({"body": "new release notes"}), encoding="utf-8")
            body.mkdir()
            full.write_bytes(b"previous full notes")
            with self.assertRaises(OSError):
                prepare_release_notes.prepare_notes(source, body, full)
            self.assertEqual(full.read_bytes(), b"previous full notes")
            self.assertEqual(list(body.iterdir()), [])

    @unittest.skipUnless(os.name == "nt", "Windows read-only output semantics")
    def test_readonly_output_preserves_both_note_files(self):
        for target in ("body.md", "full.md"):
            with self.subTest(target=target), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                source, body, full = root / "notes.json", root / "body.md", root / "full.md"
                source.write_text(json.dumps({"body": "new release notes"}), encoding="utf-8")
                body.write_bytes(b"previous body")
                full.write_bytes(b"previous full notes")
                readonly = root / target
                readonly.chmod(stat.S_IREAD)
                try:
                    with self.assertRaises(OSError):
                        prepare_release_notes.prepare_notes(source, body, full)
                    self.assertEqual(body.read_bytes(), b"previous body")
                    self.assertEqual(full.read_bytes(), b"previous full notes")
                finally:
                    readonly.chmod(stat.S_IREAD | stat.S_IWRITE)


class ReleaseNotesTests(unittest.TestCase):
    def test_short_notes_are_unchanged_including_unicode_at_limit(self):
        for notes in ("", "## What's Changed\n* Fix\n", "я" * 125000):
            self.assertEqual(prepare_release_notes.release_body(notes), notes)

    def test_only_changes_section_is_removed_from_long_notes(self):
        for newline in ("\n", "\r\n"):
            for tail in ("## New Contributors\n* @example\n\n**Full Changelog**: URL\n",
                         "**Full Changelog**: URL\n", ""):
                with self.subTest(newline=newline, tail=tail):
                    prefix = "Release introduction" + newline
                    tail = tail.replace("\n", newline)
                    notes = prefix + "## What's Changed" + newline + "* change\n" * 20000 + tail
                    self.assertEqual(prepare_release_notes.release_body(notes), prefix + tail)

    def test_oversized_remainder_is_not_shortened(self):
        remainder = "## New Contributors\n" + "x" * 125001
        notes = "## What's Changed\n* Fix\n\n" + remainder
        self.assertEqual(prepare_release_notes.release_body(notes), remainder)
        self.assertEqual(prepare_release_notes.release_body(remainder), remainder)

    def test_full_notes_are_preserved_in_asset(self):
        for notes in ("", "## What's Changed\n* Fix\n", "я" * 125000,
                      "## What's Changed\n" + "* Fix\n" * 25000 + "## New Contributors\n* @example\n"):
            with self.subTest(length=len(notes)), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                source = root / "notes.json"
                body = root / "body.md"
                full = root / "dist/release-notes.md"
                source.write_text(json.dumps({"body": notes}), encoding="utf-8")
                prepare_release_notes.prepare_notes(source, body, full)
                self.assertEqual(full.read_bytes(), notes.encode("utf-8"))
                self.assertEqual(body.read_bytes(), prepare_release_notes.release_body(notes).encode("utf-8"))


if __name__ == "__main__":
    unittest.main()
