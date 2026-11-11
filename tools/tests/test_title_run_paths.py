import contextlib
import datetime
import importlib.util
import io
import json
import os
from pathlib import Path
import shutil
import stat
import subprocess
import sys
import tempfile
import unittest
from unittest import mock


ROOT = Path(__file__).resolve().parents[2]
SCRIPT = Path(os.environ.get("TITLE_RUN_SCRIPT", ROOT / "tools/title_run.py"))
TITLE = "PPSA00001"
OTHER = "PPSA00002"


def snapshot(root):
    found = {}
    if not root.exists() and not root.is_symlink():
        return found
    pending = [(root, ".")]
    while pending:
        path, name = pending.pop()
        details = path.lstat()
        mode = stat.S_IMODE(details.st_mode)
        reparse = os.name == "nt" and getattr(details, "st_file_attributes", 0) & stat.FILE_ATTRIBUTE_REPARSE_POINT
        if path.is_symlink() or reparse:
            found[name] = ("link", mode, os.readlink(path))
        elif path.is_dir():
            found[name] = ("directory", mode)
            pending.extend((child, str(Path(name) / child.name)) for child in path.iterdir())
        else:
            found[name] = ("file", mode, path.read_bytes())
    return found


class TitleRunPathsTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="anyps5-title-paths-")
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.out = self.root / "out"
        self.build = self.root / "build"
        self.titles = self.root / "titles.json"
        self.titles.write_text("{}", encoding="utf-8")
        self.games = []

    def package(self, path, title=TITLE):
        (path / "sce_sys").mkdir(parents=True)
        (path / "eboot.bin").write_bytes(b"\x7fELF" + bytes(60))
        (path / "assets.bin").write_bytes(b"original extracted title bytes")
        (path / "sce_sys/param.json").write_text(json.dumps({"titleId": title, "contentVersion": "01.000.000"}), encoding="utf-8")
        self.games.append(path)
        return path

    def stale_work(self, title=TITLE):
        work = self.out / "work" / title
        work.mkdir(parents=True, exist_ok=True)
        (work / "previous-run.bin").write_bytes(b"preserve this until all paths pass preflight")
        return work

    def symlink(self, target, link, directory=False):
        link.parent.mkdir(parents=True, exist_ok=True)
        try:
            link.symlink_to(target, target_is_directory=directory)
        except (OSError, NotImplementedError) as error:
            self.skipTest(f"symlinks unavailable: {error}")

    def command(self, ids=(TITLE,), *, default_out=False, extra=()):
        arguments = [sys.executable, str(SCRIPT), "--build", str(self.build), "--titles", str(self.titles), "--runs", "1", "--seconds", "1"]
        if not default_out:
            arguments.extend(("--out", str(self.out)))
        for package in self.games:
            arguments.extend(("--games", str(package)))
        return arguments + list(extra) + list(ids)

    def run_cli(self, ids=(TITLE,), *, default_out=False, extra=(), frozen_time=None):
        command = self.command(ids, default_out=default_out, extra=extra)
        if frozen_time is not None:
            bootstrap = """import datetime
import runpy
import sys
frozen = datetime.datetime.fromisoformat(sys.argv.pop(1))
class FrozenDateTime(datetime.datetime):
    @classmethod
    def now(cls, tz=None):
        return cls.fromtimestamp(frozen.timestamp(), tz)
datetime.datetime = FrozenDateTime
sys.argv = sys.argv[1:]
runpy.run_path(sys.argv[0], run_name="__main__")
"""
            command = [command[0], "-c", bootstrap, frozen_time.isoformat(), *command[1:]]
        return subprocess.run(command, cwd=self.root, capture_output=True, text=True, timeout=15)

    def refuse(self, ids=(TITLE,), *, extra=()):
        before = snapshot(self.root)
        result = self.run_cli(ids, extra=extra)
        self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
        self.assertRegex(result.stderr.lower(), r"overlap|unsafe")
        self.assertEqual(snapshot(self.root), before, result.stdout + result.stderr)

    def fake_relinker(self):
        target = self.build / "core/relinker" / ("relinker.exe" if os.name == "nt" else "relinker")
        target.parent.mkdir(parents=True)
        shutil.copy2(sys.executable, target)
        (self.build / "CMakeCache.txt").write_text(f"CMAKE_HOME_DIRECTORY:INTERNAL={ROOT}\n", encoding="utf-8")
        return target

    def reports(self, output=None):
        output = output or self.out
        return sorted(path for path in output.iterdir() if path.is_dir() and not path.is_symlink() and path.name != "work")

    def test_package_equals_work_directory(self):
        self.package(self.out / "work" / TITLE)
        self.refuse()

    def test_package_is_below_work_directory(self):
        self.package(self.out / "work" / TITLE / "original-dump")
        self.refuse()

    def test_output_is_below_package(self):
        package = self.package(self.root / "games" / TITLE)
        self.out = package / "reports"
        self.refuse()

    def test_unselected_package_is_protected(self):
        self.package(self.root / "games" / TITLE)
        self.package(self.out / "work" / TITLE / "unselected", OTHER)
        self.refuse()

    def test_preflight_checks_later_titles_before_cleaning_first(self):
        self.package(self.root / "games" / TITLE)
        self.package(self.out / "work" / OTHER, OTHER)
        self.stale_work()
        self.refuse((TITLE, OTHER))

    def test_work_parent_aliases_games_directory(self):
        package = self.package(self.root / "games" / TITLE)
        self.symlink(package.parent, self.out / "work", directory=True)
        self.refuse()

    def test_work_parent_alias_cannot_make_work_contain_output(self):
        self.package(self.root / "games" / TITLE)
        self.out = self.root / TITLE / "out"
        self.symlink(self.root, self.out / "work", directory=True)
        self.refuse()

    def test_work_directory_aliases_package(self):
        package = self.package(self.root / "games" / TITLE)
        self.symlink(package, self.out / "work" / TITLE, directory=True)
        self.refuse()

    def test_work_lock_aliases_an_input_file(self):
        package = self.package(self.root / "games" / TITLE)
        self.symlink(package / "assets.bin", self.out / "work" / f"{TITLE}.lock")
        self.refuse()

    def test_nested_asset_directory_alias_target_is_protected(self):
        package = self.package(self.root / "games" / TITLE)
        self.fake_relinker()
        target = self.stale_work() / "original-assets"
        target.mkdir()
        (target / "asset.bin").write_bytes(b"original nested alias-backed title assets")
        assets = self.root / "external-assets"
        self.symlink(target, assets / "nested", directory=True)
        self.symlink(assets, package / "assets", directory=True)
        self.refuse()

    def test_disjoint_asset_directory_aliases_are_safe(self):
        package = self.package(self.root / "games" / TITLE)
        self.fake_relinker()
        work = self.stale_work()
        assets, library = self.root / "external-assets", self.root / "external-library"
        library.mkdir()
        (library / "texture.bin").write_bytes(b"external texture bytes")
        self.symlink(library, assets / "nested", directory=True)
        self.symlink(assets, package / "assets", directory=True)
        inputs = [(path, snapshot(path)) for path in (package, assets, library)]
        result = self.run_cli()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        for path, before in inputs:
            self.assertEqual(snapshot(path), before)
        self.assertFalse((work / "previous-run.bin").exists())
        self.assertTrue((work / "input/eboot.elf").is_file())
        reports = self.reports()
        self.assertEqual(len(reports), 1)
        report = json.loads((reports[0] / "summary.json").read_text(encoding="utf-8"))
        self.assertEqual(report["titles"][TITLE]["runs"][0]["outcome"], "relink-failed")

    def test_disjoint_asset_directory_alias_cycle_is_safe(self):
        package = self.package(self.root / "games" / TITLE)
        self.fake_relinker()
        assets = self.root / "external-assets"
        self.symlink(assets, package / "assets", directory=True)
        self.symlink(package / "assets", assets / "back", directory=True)
        (assets / "asset.bin").write_bytes(b"assets behind a directory alias cycle")
        before_package, before_assets = snapshot(package), snapshot(assets)
        result = self.run_cli()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(snapshot(package), before_package)
        self.assertEqual(snapshot(assets), before_assets)
        reports = self.reports()
        self.assertEqual(len(reports), 1)
        report = json.loads((reports[0] / "summary.json").read_text(encoding="utf-8"))
        self.assertEqual(report["titles"][TITLE]["runs"][0]["outcome"], "relink-failed")

    @unittest.skipUnless(os.name == "nt", "junctions are Windows-only")
    def test_asset_directory_junction_target_is_protected(self):
        package = self.package(self.root / "games" / TITLE)
        target = self.stale_work() / "original assets \u00e9\u754c"
        target.mkdir()
        (target / "asset.bin").write_bytes(b"original junction-backed title assets")
        junction = package / "assets-junction"
        environment = dict(os.environ, TARGET=str(target), LINK=str(junction))
        try:
            result = subprocess.run(["powershell.exe", "-NoProfile", "-NonInteractive", "-WindowStyle", "Hidden", "-Command",
                                     "New-Item -ItemType Junction -Path $env:LINK -Value $env:TARGET"],
                                    env=environment, capture_output=True, text=True, encoding="utf-8", errors="replace",
                                    timeout=10, creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
        except (OSError, subprocess.SubprocessError) as error:
            self.skipTest(f"powershell unavailable: {error}")
        if result.returncode != 0 or not junction.is_dir():
            self.skipTest(f"junction creation unavailable: {result.stderr.strip()}")

        def remove_junction():
            if junction.is_dir():
                junction.rmdir()

        self.addCleanup(remove_junction)
        self.assertEqual(getattr(junction.lstat(), "st_reparse_tag", 0), 0xa0000003)
        self.assertFalse(junction.is_symlink())
        self.refuse()

    def test_titles_configuration_is_below_work_directory(self):
        self.package(self.root / "games" / TITLE)
        self.titles = self.stale_work() / "titles.json"
        self.titles.write_text("{}", encoding="utf-8")
        self.refuse()

    def test_comparison_report_is_below_work_directory(self):
        self.package(self.root / "games" / TITLE)
        comparison = self.stale_work() / "summary.json"
        comparison.write_text('{"titles": {}}', encoding="utf-8")
        self.refuse(extra=("--compare", str(comparison)))

    def test_build_is_below_work_directory(self):
        self.package(self.root / "games" / TITLE)
        self.build = self.stale_work() / "build"
        self.build.mkdir()
        (self.build / "CMakeCache.txt").write_text("original build cache", encoding="utf-8")
        self.refuse()

    def test_running_tool_checkout_is_protected_when_build_points_elsewhere(self):
        self.package(self.root / "games" / TITLE)
        self.fake_relinker()
        checkout = self.stale_work() / "tool-checkout"
        scripts = checkout / "tools"
        scripts.mkdir(parents=True)
        for path in (ROOT / "tools").glob("*.py"):
            shutil.copy2(SCRIPT if path.name == "title_run.py" else path, scripts / path.name)
        (checkout / "original-checkout.bin").write_bytes(b"the running source checkout is an input too")
        command = self.command()
        command[1] = str(scripts / "title_run.py")
        before = snapshot(self.root)
        result = subprocess.run(command, cwd=self.root, env=dict(os.environ, PYTHONDONTWRITEBYTECODE="1"),
                                capture_output=True, text=True, timeout=15)
        self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
        self.assertRegex(result.stderr.lower(), r"overlap|unsafe")
        self.assertEqual(snapshot(self.root), before)

    def test_eboot_symlink_target_is_below_work_directory(self):
        package = self.package(self.root / "games" / TITLE)
        target = self.stale_work() / "original-eboot.bin"
        (package / "eboot.bin").replace(target)
        self.symlink(target, package / "eboot.bin")
        self.refuse()

    def test_module_symlink_target_is_below_work_directory(self):
        package = self.package(self.root / "games" / TITLE)
        target = self.stale_work() / "original-module.prx"
        target.write_bytes(b"\x7fELF" + bytes(60))
        self.symlink(target, package / "sce_module/libc.prx")
        self.refuse()

    def test_module_directory_symlink_target_is_below_work_directory(self):
        package = self.package(self.root / "games" / TITLE)
        target = self.stale_work() / "original-modules"
        target.mkdir()
        (target / "libc.prx").write_bytes(b"\x7fELF" + bytes(60))
        self.symlink(target, package / "sce_module", directory=True)
        self.refuse()

    def test_module_symlink_below_aliased_directory_is_protected(self):
        package = self.package(self.root / "games" / TITLE)
        target = self.stale_work() / "original-module.prx"
        target.write_bytes(b"\x7fELF" + bytes(60))
        modules = self.root / "external-modules"
        self.symlink(target, modules / "libc.prx")
        self.symlink(modules, package / "sce_module", directory=True)
        self.refuse()

    def test_titles_symlink_target_is_below_work_directory(self):
        self.package(self.root / "games" / TITLE)
        target = self.stale_work() / "original-titles.json"
        self.titles.replace(target)
        self.symlink(target, self.titles)
        self.refuse()

    def test_nested_extra_executable_alias_target_is_protected(self):
        package = self.package(self.root / "games" / TITLE)
        target = self.stale_work() / "original-extra.elf"
        target.write_bytes(b"\x7fELF" + bytes(60))
        self.symlink(target, package / "nested/tools/extra.bin")
        self.titles.write_text(json.dumps({TITLE: {"executables": ["nested/tools/extra.bin"]}}), encoding="utf-8")
        self.refuse()

    def test_relinker_alias_target_is_protected(self):
        self.package(self.root / "games" / TITLE)
        target = self.stale_work() / "original-relinker"
        target.write_bytes(b"original converter executable")
        name = "relinker.exe" if os.name == "nt" else "relinker"
        self.symlink(target, self.build / "core/relinker" / name)
        self.refuse()

    def test_build_library_alias_target_is_protected(self):
        self.package(self.root / "games" / TITLE)
        target = self.stale_work() / "original-libc.prx"
        target.write_bytes(b"original runtime library")
        self.symlink(target, self.build / "core/libs/libs/libc.prx")
        self.refuse()

    def test_executable_paths_cannot_escape_the_package(self):
        self.package(self.root / "games" / TITLE)
        for executable in ("../outside.bin", "nested/../../outside.bin", str(self.root / "outside.bin"),
                           "..\\outside.bin", "C:\\outside.bin", "\\\\server\\share\\outside.bin"):
            with self.subTest(executable=executable):
                self.titles.write_text(json.dumps({TITLE: {"executables": [executable]}}), encoding="utf-8")
                self.refuse()

    def test_disjoint_run_preserves_dump_and_cleans_previous_work(self):
        package = self.package(self.root / "games" / TITLE)
        self.fake_relinker()
        work = self.stale_work()
        before = snapshot(package)
        result = self.run_cli()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(snapshot(package), before)
        self.assertFalse((work / "previous-run.bin").exists())
        self.assertTrue((work / "input/eboot.elf").is_file())
        reports = self.reports()
        self.assertEqual(len(reports), 1)
        report = json.loads((reports[0] / "summary.json").read_text(encoding="utf-8"))
        self.assertEqual(report["titles"][TITLE]["runs"][0]["outcome"], "relink-failed")

    def test_cleanup_unlinks_stale_app0_file_hardlinks(self):
        package = self.package(self.root / "games" / TITLE)
        self.fake_relinker()
        work = self.stale_work()
        stale = work / "app0/assets.bin"
        stale.parent.mkdir()
        try:
            os.link(package / "assets.bin", stale)
        except (OSError, NotImplementedError) as error:
            self.skipTest(f"hardlinks unavailable: {error}")
        self.assertTrue(stale.samefile(package / "assets.bin"))
        before = snapshot(package)
        result = self.run_cli()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(snapshot(package), before)
        self.assertFalse(stale.exists())
        self.assertFalse((work / "previous-run.bin").exists())
        self.assertTrue((work / "input/eboot.elf").is_file())
        reports = self.reports()
        self.assertEqual(len(reports), 1)
        report = json.loads((reports[0] / "summary.json").read_text(encoding="utf-8"))
        self.assertEqual(report["titles"][TITLE]["runs"][0]["outcome"], "relink-failed")

    def test_cleanup_unlinks_stale_app0_directory_symlink(self):
        package = self.package(self.root / "games" / TITLE)
        self.fake_relinker()
        work = self.stale_work()
        stale = work / "app0"
        self.symlink(package, stale, directory=True)
        before = snapshot(package)
        result = self.run_cli()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(snapshot(package), before)
        self.assertFalse(stale.exists())
        self.assertFalse(stale.is_symlink())
        self.assertFalse((work / "previous-run.bin").exists())
        self.assertTrue((work / "input/eboot.elf").is_file())
        reports = self.reports()
        self.assertEqual(len(reports), 1)
        report = json.loads((reports[0] / "summary.json").read_text(encoding="utf-8"))
        self.assertEqual(report["titles"][TITLE]["runs"][0]["outcome"], "relink-failed")

    def test_default_output_under_build_is_safe(self):
        package = self.package(self.root / "games" / TITLE)
        relinker = self.fake_relinker()
        before_package, before_relinker = snapshot(package), relinker.read_bytes()
        result = self.run_cli(default_out=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(snapshot(package), before_package)
        self.assertEqual(relinker.read_bytes(), before_relinker)
        reports = self.reports(self.build / "title-runs")
        self.assertEqual(len(reports), 1)
        self.assertTrue((reports[0] / "summary.json").is_file())

    def test_disjoint_output_directory_alias_is_safe(self):
        package = self.package(self.root / "games" / TITLE)
        self.fake_relinker()
        output = self.root / "physical-output"
        output.mkdir()
        self.symlink(output, self.out, directory=True)
        before = snapshot(package)
        result = self.run_cli()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(snapshot(package), before)
        self.assertTrue(self.out.is_symlink())
        self.assertEqual(len(self.reports(output)), 1)

    def test_disjoint_comparison_report_is_preserved(self):
        package = self.package(self.root / "games" / TITLE)
        self.fake_relinker()
        comparison = self.root / "previous-summary.json"
        comparison.write_text('{"titles": {}}', encoding="utf-8")
        before_package, before_comparison = snapshot(package), comparison.read_bytes()
        result = self.run_cli(extra=("--compare", str(comparison)))
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(snapshot(package), before_package)
        self.assertEqual(comparison.read_bytes(), before_comparison)
        self.assertEqual(len(self.reports()), 1)

    def test_valid_nested_extra_executable_is_staged(self):
        package = self.package(self.root / "games" / TITLE)
        self.fake_relinker()
        original = package / "nested/tools/extra.bin"
        original.parent.mkdir(parents=True)
        original.write_bytes(b"\x7fELF" + bytes(range(60)))
        self.titles.write_text(json.dumps({TITLE: {"executables": ["nested/tools/extra.bin"]}}), encoding="utf-8")
        before = snapshot(package)
        result = self.run_cli()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(snapshot(package), before)
        self.assertEqual((self.out / "work" / TITLE / "input/nested/tools/extra.bin").read_bytes(), original.read_bytes())

    def test_safe_module_directories_survive_preflight_and_cleanup(self):
        package = self.package(self.root / "games" / TITLE)
        self.fake_relinker()
        work = self.stale_work()
        module = package / "Media/Modules/plugin.sprx"
        module.parent.mkdir(parents=True)
        module.write_bytes(b"\x7fELF" + bytes(range(60)))
        (module.parent / "readme.txt").write_text("not a module", encoding="utf-8")
        (package / "Media/Empty").mkdir()
        os.link(package / "eboot.bin", package / "alias.bin")
        self.titles.write_text(json.dumps({TITLE: {"relink": ["--module-dir", ".", "--module-dir", "Media/Modules", "--module-dir", "Media/Empty"]}}), encoding="utf-8")
        inputs = [(path, snapshot(path)) for path in (package, self.build, self.titles)]
        result = self.run_cli()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        for path, before in inputs:
            self.assertEqual(snapshot(path), before)
        self.assertFalse((work / "previous-run.bin").exists())
        self.assertEqual((work / "input/Media/Modules/plugin.sprx").read_bytes(), module.read_bytes())
        self.assertTrue((work / "input/Media/Empty").is_dir())
        self.assertTrue((work / "input/eboot.elf").is_file())
        self.assertFalse((work / "input/alias.bin").exists())
        self.assertFalse((work / "input/Media/Modules/readme.txt").exists())

    def test_cleanup_failure_does_not_write_through_stale_log_alias(self):
        self.package(self.root / "games" / TITLE)
        work = self.stale_work()
        victim = self.root / "original-victim.bin"
        victim.write_bytes(b"a cleanup error must not overwrite this file")
        log = work / "relink.log"
        self.symlink(victim, log)
        specification = importlib.util.spec_from_file_location("title_run_cleanup_path_test", SCRIPT)
        runner = importlib.util.module_from_spec(specification)
        specification.loader.exec_module(runner)
        errors, output = io.StringIO(), io.StringIO()
        before = victim.read_bytes()
        handles, lock_work = [], runner.lock_work

        def tracked_lock(path):
            handle = lock_work(path)
            if handle is not None:
                handles.append(handle)
            return handle

        with mock.patch.object(runner.shutil, "rmtree", side_effect=PermissionError("test cleanup denied")), \
                mock.patch.object(runner, "prepare", wraps=runner.prepare) as prepare, \
                mock.patch.object(runner, "lock_work", side_effect=tracked_lock), \
                contextlib.redirect_stderr(errors), contextlib.redirect_stdout(output):
            try:
                code = runner.main(self.command()[2:])
            except SystemExit as error:
                code = error.code
            finally:
                for handle in handles:
                    handle.close()
        self.assertEqual(victim.read_bytes(), before, output.getvalue() + errors.getvalue())
        self.assertTrue(log.is_symlink())
        self.assertEqual(code, 2, output.getvalue() + errors.getvalue())
        self.assertIn("cannot reset", errors.getvalue().lower())
        prepare.assert_not_called()

    def report_name(self, instant):
        result = subprocess.run(["git", "-C", str(ROOT), "rev-parse", "--short", "HEAD"], capture_output=True, text=True, check=True)
        return f"{result.stdout.strip()}-{instant:%Y%m%d-%H%M%S}"

    def test_old_report_directory_symlinks_are_not_followed(self):
        package = self.package(self.root / "games" / TITLE)
        self.fake_relinker()
        self.out.mkdir()
        instant = datetime.datetime(2001, 2, 3, 4, 5, 6)
        old = self.report_name(instant)
        self.symlink(package, self.out / old, directory=True)
        before = snapshot(package)
        result = self.run_cli(frozen_time=instant)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(snapshot(package), before)
        self.assertEqual(len(self.reports()), 1)
        self.assertTrue(self.reports()[0].name.startswith(old + "-"))

    def test_old_report_file_symlinks_are_not_followed(self):
        package = self.package(self.root / "games" / TITLE)
        self.fake_relinker()
        protected = self.root / "original-summary.json"
        protected.write_bytes(b"preserve the input behind a stale report link")
        self.out.mkdir()
        instant = datetime.datetime(2001, 2, 3, 4, 5, 6)
        old = self.report_name(instant)
        report = self.out / old
        report.mkdir()
        self.symlink(protected, report / "summary.json")
        before_package, before_protected = snapshot(package), protected.read_bytes()
        result = self.run_cli(frozen_time=instant)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(snapshot(package), before_package)
        self.assertEqual(protected.read_bytes(), before_protected)
        fresh = [report for report in self.reports() if report.name != old]
        self.assertEqual(len(fresh), 1)
        self.assertTrue(fresh[0].name.startswith(old + "-"))
        self.assertFalse((fresh[0] / "summary.json").is_symlink())


if __name__ == "__main__":
    unittest.main()
