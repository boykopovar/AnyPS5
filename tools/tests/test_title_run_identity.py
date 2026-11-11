import contextlib
import importlib.util
import io
import json
import os
from pathlib import Path, PureWindowsPath
import shutil
import stat
import sys
import tempfile
import unittest
from unittest import mock


ROOT = Path(__file__).resolve().parents[2]
SCRIPT = Path(os.environ.get("TITLE_RUN_SCRIPT", ROOT / "tools/title_run.py"))
TITLE = "PPSA00001"


def snapshot(root):
    return {str(path.relative_to(root)): None if path.is_dir() else path.read_bytes() for path in root.rglob("*")}


class TitleRunIdentityTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="anyps5-title-identity-")
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.build = self.root / "build"
        self.out = self.root / "out"
        self.titles = self.root / "titles.json"
        self.titles.write_text("{}", encoding="utf-8")
        self.games = []
        specification = importlib.util.spec_from_file_location("title_run_identity_test", SCRIPT)
        self.runner = importlib.util.module_from_spec(specification)
        specification.loader.exec_module(self.runner)

    def package(self, path):
        (path / "sce_sys").mkdir(parents=True)
        (path / "eboot.bin").write_bytes(b"\x7fELF" + bytes(60))
        (path / "asset.bin").write_bytes(b"original input bytes")
        (path / "sce_sys/param.json").write_text(json.dumps({"titleId": TITLE}), encoding="utf-8")
        self.games.append(path)
        return path

    def arguments(self):
        arguments = ["--build", str(self.build), "--out", str(self.out), "--titles", str(self.titles), "--runs", "1", "--seconds", "1"]
        for package in self.games:
            arguments.extend(("--games", str(package)))
        return arguments + [TITLE]

    @contextlib.contextmanager
    def equivalent_stat(self, alias, physical):
        with self.equivalent_paths(((alias, physical),)):
            yield

    @contextlib.contextmanager
    def equivalent_paths(self, aliases):
        original_stat, original_open = os.stat, os.open

        def mapped(path):
            if isinstance(path, int):
                return path
            candidate = Path(os.fsdecode(path))
            for alias, physical in sorted(aliases, key=lambda pair: len(pair[0].parts), reverse=True):
                if candidate.is_relative_to(alias):
                    return physical / candidate.relative_to(alias)
            return candidate

        def stat(path, *arguments, **keywords):
            return original_stat(mapped(path), *arguments, **keywords)

        def open_directory(path, *arguments, **keywords):
            return original_open(mapped(path), *arguments, **keywords)

        with mock.patch.object(os, "stat", side_effect=stat), mock.patch.object(os, "open", side_effect=open_directory):
            yield

    @contextlib.contextmanager
    def directory_mounts(self, mounts):
        if not hasattr(self.runner, "PathChecks"):
            yield
            return
        original_directory = self.runner.PathChecks.directory

        def directory(checks, path):
            result = original_directory(checks, path)
            if result is not None:
                for prefix, mount in sorted(mounts.items(), key=lambda pair: len(pair[0].parts), reverse=True):
                    if path.is_relative_to(prefix):
                        return result[0], mount
            return result

        with mock.patch.object(self.runner.PathChecks, "directory", directory):
            yield

    def refuse_layout(self, reason=r"overlap|mount boundary|ambiguous", extra_args=()):
        before = snapshot(self.root)
        errors, output = io.StringIO(), io.StringIO()
        with mock.patch.object(self.runner, "reset_work", wraps=self.runner.reset_work) as cleanup, \
                mock.patch.object(self.runner, "lock_work", side_effect=AssertionError("unsafe identity alias reached lock creation")) as locking, \
                contextlib.redirect_stderr(errors), contextlib.redirect_stdout(output):
            try:
                code = self.runner.main([*self.arguments(), *extra_args])
            except SystemExit as error:
                code = error.code
        diagnostic = output.getvalue() + errors.getvalue()
        self.assertEqual(code, 2, diagnostic)
        self.assertIn("unsafe title-run layout", errors.getvalue(), diagnostic)
        self.assertRegex(errors.getvalue(), reason)
        cleanup.assert_not_called()
        locking.assert_not_called()
        self.assertEqual(snapshot(self.root), before, diagnostic)

    def refuse_alias(self, alias, physical, mounts=None):
        with self.equivalent_stat(alias, physical), self.directory_mounts(mounts or {}):
            self.refuse_layout()

    def stale_work(self):
        work = self.out / "work" / TITLE
        work.mkdir(parents=True)
        (work / "previous-run.bin").write_bytes(b"preserve stale work until every input is protected")
        return work

    def test_module_directories_cannot_escape_before_cleanup(self):
        package = self.package(self.root / "games" / TITLE)
        work = self.stale_work()
        (work / "plugin.sprx").write_bytes(b"\x7fELF" + bytes(60))
        for directory in (os.path.relpath(work, package), str(work), "Media/../../outside",
                          "..\\outside", "C:\\outside", "\\\\server\\share\\outside"):
            with self.subTest(directory=directory):
                self.titles.write_text(json.dumps({TITLE: {"relink": ["--module-dir", directory]}}), encoding="utf-8")
                self.refuse_layout(r"unsafe module directory")

    def test_content_file_below_work_is_refused_before_cleanup(self):
        self.package(self.root / "games" / TITLE)
        content = self.stale_work() / "content"
        (content / "media").mkdir(parents=True)
        (content / "media/asset.bin").write_bytes(b"preserve external content")
        self.titles.write_text(json.dumps({TITLE: {"files": {"app0/asset.bin": {"content": "media/asset.bin"}}}}), encoding="utf-8")
        self.refuse_layout(r"overlap", ("--content", str(content)))

    def test_content_directory_identity_alias_is_refused_before_cleanup(self):
        self.package(self.root / "games" / TITLE)
        work = self.stale_work()
        (work / "asset.bin").write_bytes(b"preserve aliased content")
        alias = self.root / "content-view"
        self.titles.write_text(json.dumps({TITLE: {"files": {"app0/asset.bin": {"content": "asset.bin"}}}}), encoding="utf-8")
        with self.equivalent_stat(alias, work):
            self.refuse_layout(r"overlap", ("--content", str(alias)))

    def test_content_root_containing_work_allows_disjoint_consumed_files(self):
        package = self.package(self.root / "games" / TITLE)
        work = self.stale_work()
        (self.root / "external.bin").write_bytes(b"disjoint original content")
        before = snapshot(self.root)
        for relative in ("external.bin", "missing.bin"):
            with self.subTest(relative=relative):
                args = self.runner.argparse.Namespace(titles=self.titles, build=self.build, out=self.out, compare=None, content=self.root)
                titles = {TITLE: {"files": {"app0/asset.bin": {"content": relative}, "data/config.txt": {"text": str(work / "inline.txt")}}}}
                self.runner.validate_layout(args, {TITLE: {"package": package}}, [TITLE], titles, self.root / "source", "windows" if os.name == "nt" else "linux")
        self.assertEqual(snapshot(self.root), before)

    @unittest.skipUnless(os.name == "posix", "POSIX spelling model requires POSIX paths")
    def test_case_alias_of_package_is_refused_before_cleanup(self):
        physical, alias = self.root / "TitleRuns", self.root / "titleruns"
        self.package(physical / "work" / TITLE)
        self.out = alias
        self.refuse_alias(alias, physical)

    @unittest.skipUnless(os.name == "posix", "POSIX spelling model requires POSIX paths")
    def test_unicode_alias_of_package_is_refused_before_cleanup(self):
        physical, alias = self.root / "caf\u00e9", self.root / "cafe\u0301"
        self.package(physical / "work" / TITLE)
        self.out = alias
        self.refuse_alias(alias, physical)

    @unittest.skipUnless(os.name == "posix", "POSIX spelling model requires POSIX paths")
    def test_missing_output_descendant_of_case_aliased_package_is_refused(self):
        physical, alias = self.root / "Package", self.root / "package"
        self.package(physical)
        self.out = alias / "missing" / "reports"
        self.refuse_alias(alias, physical)

    @unittest.skipUnless(os.name == "posix", "POSIX spelling model requires POSIX paths")
    def test_missing_output_descendant_of_unicode_aliased_package_is_refused(self):
        physical, alias = self.root / "caf\u00e9", self.root / "cafe\u0301"
        self.package(physical)
        self.out = alias / "missing" / "reports"
        self.refuse_alias(alias, physical)

    @unittest.skipUnless(os.name == "posix", "POSIX spelling model requires POSIX paths")
    def test_case_alias_of_titles_configuration_is_refused_before_cleanup(self):
        self.package(self.root / "games" / TITLE)
        physical, alias = self.root / "TitleRuns", self.root / "titleruns"
        self.titles = physical / "work" / TITLE / "configuration.json"
        self.titles.parent.mkdir(parents=True)
        self.titles.write_text("{}", encoding="utf-8")
        self.out = alias
        self.refuse_alias(alias, physical)

    def test_package_mounted_below_work_is_refused_before_cleanup(self):
        package = self.package(self.root / "games" / TITLE)
        work = self.out / "work" / TITLE
        mounted = work / "mounted-package"
        mounted.mkdir(parents=True)
        (work / "previous-run.bin").write_bytes(b"preserve stale work until every input is protected")
        self.refuse_alias(mounted, package, {mounted: -1} if sys.platform == "linux" else {})

    def test_package_assets_mounted_from_work_are_refused_before_cleanup(self):
        package = self.package(self.root / "games" / TITLE)
        mounted = package / "mounted-assets"
        mounted.mkdir()
        assets = self.out / "work" / TITLE / "original-assets"
        assets.mkdir(parents=True)
        (assets / "original.bin").write_bytes(b"original mounted title asset bytes")
        self.refuse_alias(mounted, assets, {mounted: -1} if sys.platform == "linux" else {})

    def test_output_bind_view_of_package_assets_is_refused_before_first_run(self):
        package = self.package(self.root / "games" / TITLE)
        assets = package / "ordinary-assets"
        assets.mkdir()
        (assets / "original.bin").write_bytes(b"original package assets")
        self.out = self.root / "bound-output"
        self.refuse_alias(self.out, assets, {self.out: -1} if sys.platform == "linux" else {})

    def test_missing_output_below_package_asset_bind_view_is_refused_before_first_run(self):
        package = self.package(self.root / "games" / TITLE)
        assets = package / "ordinary-assets"
        assets.mkdir()
        (assets / "original.bin").write_bytes(b"original package assets")
        alias = self.root / "bound-assets"
        self.out = alias / "missing-reports"
        self.refuse_alias(alias, assets, {alias: -1} if sys.platform == "linux" else {})

    def test_windows_case_sensitive_paths_and_identity_caches_remain_distinct(self):
        directories = {}

        class WindowsDirectory(PureWindowsPath):
            def stat(self):
                if self.parts not in directories:
                    raise FileNotFoundError(str(self))
                return os.stat_result((stat.S_IFDIR, directories[self.parts], 1, 1, 0, 0, 0, 0, 0, 0))

        first = WindowsDirectory("C:/review/TitleRuns/work/PPSA00001")
        second = WindowsDirectory("C:/review/titleruns/work/PPSA00001")
        for second_inode in (200, 100):
            directories.update({first.parts: 100, second.parts: second_inode})
            with self.subTest(second_inode=second_inode), mock.patch.object(self.runner.sys, "platform", "darwin"):
                checks = self.runner.PathChecks()
                self.assertEqual(checks.identity(first), (1, 100))
                self.assertEqual(checks.identity(second), (1, second_inode))
                self.assertEqual(checks.directory(first), ((1, 100), None))
                self.assertEqual(checks.directory(second), ((1, second_inode), None))
                self.assertEqual(checks.same(first, second), second_inode == 100)
                self.assertEqual(checks.contains(first, second), second_inode == 100)
                self.assertEqual(checks.contains(first, second / "missing"), second_inode == 100)
                self.assertEqual(checks.contains(second, first / "missing"), second_inode == 100)
                self.assertTrue(checks.contains(first, first / "missing"))

    def test_windows_path_locations_preserve_case_distinct_spellings(self):
        first = PureWindowsPath("C:/review/TitleRuns")
        second = PureWindowsPath("C:/review/titleruns")
        with mock.patch.object(self.runner, "Path", PureWindowsPath), \
                mock.patch.object(os.path, "abspath", return_value=str(first)), \
                mock.patch.object(self.runner, "comparison_path", return_value=second):
            locations = self.runner.path_locations(first)
        self.assertEqual([path.parts for path in locations], [first.parts, second.parts])

    def test_windows_case_sensitive_symlink_chain_distinguishes_a_real_cycle(self):
        links = {}

        class WindowsLink(PureWindowsPath):
            def is_symlink(self):
                return self.parts in links

            def readlink(self):
                return WindowsLink(links[self.parts])

            def resolve(self):
                return self

        first = WindowsLink("C:/review/link")
        second = WindowsLink("C:/review/Link")
        target = WindowsLink("C:/physical")
        links.update({first.parts: second, second.parts: target})
        try:
            resolved = self.runner.comparison_path(first / "asset.bin")
        except RuntimeError as error:
            self.fail(f"valid case-sensitive symlink chain was refused: {error}")
        self.assertEqual(resolved.parts, (target / "asset.bin").parts)
        links[second.parts] = first
        with self.assertRaisesRegex(RuntimeError, "symbolic link cycle"):
            self.runner.comparison_path(first / "asset.bin")

    @unittest.skipUnless(sys.platform == "linux", "mount IDs are Linux-specific")
    def test_work_root_bind_mount_is_refused_before_cleanup(self):
        self.package(self.root / "games" / TITLE)
        work = self.stale_work()
        for native_mount in (None, 10001):
            native = contextlib.nullcontext() if native_mount is None else self.fdinfo_contents(f"mnt_id: {native_mount}\n")
            with self.subTest(native_mount=native_mount), native, self.directory_mounts({work: -1}):
                self.refuse_layout(r"mount boundary")

    @unittest.skipUnless(sys.platform == "linux", "mount IDs are Linux-specific")
    def test_work_parent_bind_mount_is_refused_before_cleanup(self):
        self.package(self.root / "games" / TITLE)
        work = self.stale_work()
        for native_mount in (None, 10001):
            native = contextlib.nullcontext() if native_mount is None else self.fdinfo_contents(f"mnt_id: {native_mount}\n")
            with self.subTest(native_mount=native_mount), native, self.directory_mounts({work.parent: -1}):
                self.refuse_layout(r"mount boundary")

    @unittest.skipUnless(sys.platform == "linux", "mount IDs are Linux-specific")
    def test_nested_bind_mount_is_refused_before_cleanup(self):
        self.package(self.root / "games" / TITLE)
        mounted = self.stale_work() / "mounted-assets"
        mounted.mkdir()
        for native_mount in (None, 10001):
            native = contextlib.nullcontext() if native_mount is None else self.fdinfo_contents(f"mnt_id: {native_mount}\n")
            with self.subTest(native_mount=native_mount), native, self.directory_mounts({mounted: -1}):
                self.refuse_layout(r"mount boundary")

    @unittest.skipUnless(sys.platform == "linux", "mount IDs are Linux-specific")
    def test_mount_boundary_is_checked_before_repeated_inode_is_skipped(self):
        self.package(self.root / "games" / TITLE)
        work = self.stale_work()
        mounted = work / "same-inode-new-mount"
        mounted.mkdir()
        with self.equivalent_stat(mounted, work), self.directory_mounts({mounted: -1}):
            self.refuse_layout(r"mount boundary")

    @unittest.skipUnless(sys.platform == "linux", "mount IDs are Linux-specific")
    def test_same_inode_input_mount_views_are_both_inspected(self):
        package = self.package(self.root / "games" / TITLE)
        assets = self.stale_work() / "original-assets"
        assets.mkdir()
        (assets / "original.bin").write_bytes(b"assets visible through only one mount view")
        views = (package / "first-view", package / "second-view")
        for view in views:
            (view / "nested-assets").mkdir(parents=True)
        with os.scandir(package) as entries:
            pending = [package / entry.name for entry in entries if entry.is_dir()]
        scanned = pending[-1]
        other = next(view for view in views if view != scanned)
        aliases = ((other / "nested-assets", assets), (other, scanned))
        mounts = {scanned: -1, other: -2, other / "nested-assets": -3}
        with self.equivalent_paths(aliases), self.directory_mounts(mounts):
            self.refuse_layout(r"overlaps an input directory")

    @contextlib.contextmanager
    def fdinfo_contents(self, contents=None, error=None):
        original_read = Path.read_text

        def read(path, *arguments, **keywords):
            if str(path).startswith("/proc/self/fdinfo/"):
                if error is not None:
                    raise error
                return contents
            return original_read(path, *arguments, **keywords)

        with mock.patch.object(Path, "read_text", read):
            yield

    @contextlib.contextmanager
    def track_directory_descriptors(self):
        original_open, original_close = os.open, os.close
        descriptors = []

        def open_directory(path, flags, *arguments, **keywords):
            self.assertEqual(flags & os.O_ACCMODE, os.O_RDONLY)
            self.assertTrue(flags & os.O_DIRECTORY)
            self.assertTrue(flags & os.O_CLOEXEC)
            descriptor = original_open(path, flags, *arguments, **keywords)
            descriptors.append(descriptor)
            return descriptor

        with mock.patch.object(os, "open", side_effect=open_directory), \
                mock.patch.object(os, "close", wraps=original_close) as close:
            yield descriptors
        self.assertTrue(descriptors)
        self.assertEqual([call.args[0] for call in close.call_args_list], descriptors)
        for descriptor in descriptors:
            with self.assertRaises(OSError):
                os.fstat(descriptor)

    @unittest.skipUnless(sys.platform == "linux", "fdinfo is Linux-specific")
    def test_missing_denied_or_unreadable_fdinfo_refuses_before_locks_and_closes_descriptors(self):
        self.package(self.root / "games" / TITLE)
        self.stale_work()
        errors = (FileNotFoundError("missing fdinfo"), PermissionError("denied fdinfo"),
                  UnicodeDecodeError("ascii", b"\xff", 0, 1, "non-ASCII fdinfo"))
        for error in errors:
            with self.subTest(error=type(error).__name__), self.fdinfo_contents(error=error), self.track_directory_descriptors():
                self.refuse_layout(r"readable /proc/self/fdinfo is required")

    @unittest.skipUnless(sys.platform == "linux", "fdinfo is Linux-specific")
    def test_malformed_fdinfo_refuses_before_locks_and_closes_descriptors(self):
        self.package(self.root / "games" / TITLE)
        self.stale_work()
        for contents in ("", "pos: 0\n", "mnt_id:\n", "mnt_id: -1\n", "mnt_id: invalid\n",
                         "mnt_id: 7 extra\n", "mnt_id: 7\nmnt_id: 8\n"):
            with self.subTest(contents=contents), self.fdinfo_contents(contents), self.track_directory_descriptors():
                self.refuse_layout(r"readable /proc/self/fdinfo is required")

    @unittest.skipUnless(sys.platform == "linux", "fdinfo is Linux-specific")
    def test_directory_identity_and_mount_are_cached_from_one_descriptor(self):
        expected = self.root.stat()
        checks = self.runner.PathChecks()
        with self.fdinfo_contents("pos:\t0\nmnt_id:\t12345\nino:\t7\n"), self.track_directory_descriptors() as descriptors:
            first = checks.directory(self.root)
            second = checks.directory(self.root)
            self.assertEqual(first, ((expected.st_dev, expected.st_ino), 12345))
            self.assertEqual(second, first)
            self.assertEqual(checks.identity(self.root), first[0])
            self.assertEqual(len(descriptors), 1)

    @unittest.skipUnless(sys.platform == "linux", "directory descriptors are Linux-specific")
    def test_fstat_failure_closes_directory_descriptor(self):
        checks = self.runner.PathChecks()
        with self.track_directory_descriptors(), mock.patch.object(os, "fstat", side_effect=PermissionError("fstat denied")):
            with self.assertRaisesRegex(PermissionError, "fstat denied"):
                checks.directory(self.root)

    def test_missing_directory_and_regular_file_have_no_directory_identity(self):
        checks = self.runner.PathChecks()
        self.assertIsNone(checks.directory(self.root / "absent"))
        self.assertIsNone(checks.directory(self.titles))

    @unittest.skipUnless(sys.platform == "linux", "native fdinfo is Linux-specific")
    def test_native_read_only_existing_mount_boundary_is_detected(self):
        checks = self.runner.PathChecks()
        root, proc = checks.directory(Path("/")), checks.directory(Path("/proc"))
        if root is None or proc is None or root[1] == proc[1]:
            self.skipTest("a distinct mounted proc filesystem is unavailable")
        self.assertIsInstance(root[1], int)
        self.assertIsInstance(proc[1], int)
        with self.assertRaisesRegex(ValueError, "mount boundary"):
            self.runner.check_work_directories(Path("/proc/sys"), Path("/"), set(), checks)

    def test_non_linux_directory_identity_does_not_require_proc(self):
        expected = self.root.stat()
        with mock.patch.object(self.runner.sys, "platform", "darwin"), \
                mock.patch.object(os, "open", side_effect=AssertionError("non-Linux directory opened")):
            checks = self.runner.PathChecks()
            self.assertEqual(checks.directory(self.root), ((expected.st_dev, expected.st_ino), None))

    def run_distinct_layout(self, physical_name, output_name):
        physical, self.out = self.root / physical_name, self.root / output_name
        physical.mkdir()
        self.out.mkdir(exist_ok=True)
        if physical.samefile(self.out):
            self.skipTest("this filesystem treats the spelling pair as identical")
        self.assertNotEqual((physical.stat().st_dev, physical.stat().st_ino), (self.out.stat().st_dev, self.out.stat().st_ino))
        package = self.package(physical / "work" / TITLE)
        before = snapshot(package)
        relinker = self.build / "core/relinker" / ("relinker.exe" if os.name == "nt" else "relinker")
        relinker.parent.mkdir(parents=True)
        shutil.copy2(sys.executable, relinker)
        (self.build / "CMakeCache.txt").write_text(f"CMAKE_HOME_DIRECTORY:INTERNAL={ROOT}\n", encoding="utf-8")
        errors, output = io.StringIO(), io.StringIO()
        handles, lock_work = [], self.runner.lock_work

        def track_lock(path):
            handle = lock_work(path)
            if handle is not None:
                handles.append(handle)
            return handle

        with mock.patch.object(self.runner, "lock_work", side_effect=track_lock), \
                contextlib.redirect_stderr(errors), contextlib.redirect_stdout(output):
            try:
                code = self.runner.main(self.arguments())
            finally:
                for handle in handles:
                    handle.close()
        self.assertEqual(code, 0, output.getvalue() + errors.getvalue())
        self.assertEqual(snapshot(package), before)
        self.assertEqual((self.out / "work" / TITLE / "input/eboot.elf").read_bytes(), (package / "eboot.bin").read_bytes())
        reports = list(self.out.glob("*/summary.json"))
        self.assertEqual(len(reports), 1)
        self.assertEqual(json.loads(reports[0].read_text(encoding="utf-8"))["titles"][TITLE]["runs"][0]["outcome"], "relink-failed")

    def test_distinct_case_spelling_and_identity_is_allowed(self):
        self.run_distinct_layout("TitleRuns", "titleruns")

    def test_distinct_unicode_spelling_and_identity_is_allowed(self):
        self.run_distinct_layout("caf\u00e9", "cafe\u0301")


if __name__ == "__main__":
    unittest.main()
