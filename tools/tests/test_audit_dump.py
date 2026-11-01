import contextlib
import io
import locale
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path, PureWindowsPath
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import audit_dump
import unwrap_self

SELF = b"\x4f\x15\x3d\x1d" + b"\0" * 60
ELF = b"\x7fELF" + b"\0" * 60


def fake_unwrap(source, destination, verbose=False):
    data = Path(source).read_bytes()
    if not data.startswith(SELF[:4]):
        raise unwrap_self.Refused("not a SELF container")
    Path(destination).parent.mkdir(parents=True, exist_ok=True)
    Path(destination).write_bytes(ELF + Path(source).name.encode())
    return len(data)


class AuditDumpTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.root = Path(self.directory.name)
        self.dump = self.root / "app0"
        self.dump.mkdir()
        (self.dump / "eboot.bin").write_bytes(SELF)
        self.relinker = self.root / "relinker.exe"
        self.relinker.write_bytes(b"")
        patcher = mock.patch.object(unwrap_self, "unwrap", side_effect=fake_unwrap)
        patcher.start()
        self.addCleanup(patcher.stop)

    def tearDown(self):
        self.directory.cleanup()

    def module(self, relative, data=SELF):
        path = self.dump / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)

    def test_every_module_location_is_staged_and_the_extra_ones_become_module_dirs(self):
        self.module("sce_module/libc.prx")
        self.module("Media/Modules/Il2CppUserAssemblies.prx")
        self.module("Media/Plugins/PSNCore.prx", ELF)
        self.module("libfmod.prx")
        self.module("Media/Plugins/readme.txt", b"text")
        (self.dump / "sce_modules").mkdir()
        source = self.root / "work" / "source"
        directories, extra = audit_dump.prepare(self.dump, source, log=lambda text: None)
        self.assertTrue((source / "eboot.elf").read_bytes().startswith(ELF))
        self.assertTrue((source / "sce_module" / "libc.prx").read_bytes().endswith(b"libc.prx"))
        self.assertEqual((source / "Media/Plugins/PSNCore.prx").read_bytes(), ELF)
        self.assertTrue((source / "libfmod.prx").is_file())
        self.assertFalse((source / "Media/Plugins/readme.txt").exists())
        self.assertEqual(extra, ["Media/Modules", "Media/Plugins", "."])
        self.assertEqual(directories, [source / "sce_module", source / "Media/Modules", source / "Media/Plugins", source])

    def test_a_module_that_is_neither_a_self_nor_an_elf_is_refused(self):
        self.module("sce_module/broken.prx", b"MZ" + b"\0" * 62)
        with self.assertRaises(unwrap_self.Refused):
            audit_dump.prepare(self.dump, self.root / "source", log=lambda text: None)

    def test_the_relinker_gets_the_registry_windows_and_module_dir_options(self):
        with mock.patch.object(subprocess, "run", return_value=subprocess.CompletedProcess([], 0, "", "")) as run:
            audit_dump.convert(self.relinker, self.root / "source", self.root / "out" / "app.exe", ["Media/Modules", "."], True)
        command = run.call_args.args[0]
        self.assertEqual(command[:3], [str(self.relinker), "--registry", "--windows"])
        self.assertEqual(command[3:7], ["--module-dir", "Media/Modules", "--module-dir", "."])
        self.assertEqual(command[7:], [str(self.root / "source" / "eboot.elf"), str(self.root / "out" / "app.exe")])

    def run_main(self, *arguments):
        errors = io.StringIO()
        printed = io.StringIO()
        with contextlib.redirect_stderr(errors), contextlib.redirect_stdout(printed):
            code = audit_dump.main([str(argument) for argument in arguments])
        return code, errors.getvalue()

    def test_conversion_decodes_both_streams_as_utf8_under_a_legacy_locale(self):
        message = "module-\u754c-\U0001f3ae.prx"
        payload = message.encode("utf-8")
        child = f"import os; os.write(1, {payload!r}); os.write(2, {payload!r})"
        run = subprocess.run

        def relink(command, **options):
            return run([sys.executable, "-c", child], timeout=5, **options)

        with mock.patch.object(locale, "getpreferredencoding", return_value="cp1252"), \
                mock.patch.object(locale, "getencoding", return_value="cp1252", create=True), \
                mock.patch.object(subprocess, "run", side_effect=relink):
            result = audit_dump.convert(self.relinker, self.root / "source", self.root / "app.elf", [], False)
        self.assertEqual(result.returncode, 0)
        self.assertEqual(result.stdout, message)
        self.assertEqual(result.stderr, message)

    def test_non_utf8_diagnostic_bytes_preserve_the_conversion_failure(self):
        payload = b"FAIL: native diagnostic \xff\n"
        run = subprocess.run
        for stream in (1, 2):
            with self.subTest(stream=stream):
                child = f"import os, sys; os.write({stream}, {payload!r}); sys.exit(3)"

                def relink(command, **options):
                    return run([sys.executable, "-c", child], timeout=5, **options)

                with mock.patch.object(locale, "getpreferredencoding", return_value="utf-8"), \
                        mock.patch.object(locale, "getencoding", return_value="utf-8", create=True), \
                        mock.patch.object(subprocess, "run", side_effect=relink), \
                        mock.patch.object(audit_dump.import_audit, "main") as audit:
                    code, errors = self.run_main(self.dump, "--relinker", self.relinker, "--libs", "libs")
                self.assertEqual(code, 2)
                self.assertIn("relinker exited with 3", errors)
                self.assertIn("FAIL: native diagnostic \ufffd", errors)
                audit.assert_not_called()

    def test_the_audit_gets_every_registry_library_and_module_directory(self):
        self.module("sce_module/libc.prx")
        work = self.root / "work"

        def relink(command, **options):
            output = Path(command[-1]).parent
            (output / "app.registry.json").write_text("[]")
            (output / "app.libc.prx.guest.prx.registry.json").write_text("[]")
            (output / "app.libc.prx.guest.prx.needed.json").write_text("{}")
            return subprocess.CompletedProcess(command, 0, "", "")

        with mock.patch.object(subprocess, "run", side_effect=relink), mock.patch.object(audit_dump.import_audit, "main", return_value=1) as audit:
            code, _ = self.run_main(self.dump, "--relinker", self.relinker, "--libs", "libs-a", "--libs", "libs-b", "--work", work, "--json", "report.json")
        self.assertEqual(code, 1)
        argv = audit.call_args.args[0]
        output = work / "output"
        self.assertEqual(argv[:2], [str(output / "app.libc.prx.guest.prx.registry.json"), str(output / "app.registry.json")])
        self.assertEqual(argv[2:], ["--libs", "libs-a", "--libs", "libs-b", "--modules", str(work / "source" / "sce_module"), "--json", "report.json"])

    def test_a_reused_work_directory_does_not_audit_registries_left_by_an_earlier_run(self):
        work = self.root / "work"
        stale = work / "output" / "app.old.prx.guest.prx.registry.json"
        stale.parent.mkdir(parents=True)
        stale.write_text("[]")
        (work / "source" / "sce_module").mkdir(parents=True)
        (work / "source" / "sce_module" / "gone.prx").write_bytes(ELF)

        def relink(command, **options):
            (Path(command[-1]).parent / "app.registry.json").write_text("[]")
            return subprocess.CompletedProcess(command, 0, "", "")

        with mock.patch.object(subprocess, "run", side_effect=relink), mock.patch.object(audit_dump.import_audit, "main", return_value=0) as audit:
            code, _ = self.run_main(self.dump, "--relinker", self.relinker, "--libs", "libs", "--work", work)
        self.assertEqual(code, 0)
        self.assertEqual(audit.call_args.args[0][:2], [str(work / "output" / "app.registry.json"), "--libs"])
        self.assertFalse((work / "source" / "sce_module" / "gone.prx").exists())

    def test_an_undeletable_stale_registry_stops_before_conversion(self):
        work = self.root / "work"
        stale = work / "output" / "locked" / "stale.registry.json"
        stale.parent.mkdir(parents=True)
        stale.write_text("[]")
        real_rmtree = audit_dump.shutil.rmtree

        def rmtree(path, ignore_errors=False, **kwargs):
            if Path(path) == work / "output":
                if ignore_errors:
                    return None
                raise PermissionError(13, "Permission denied", str(stale))
            return real_rmtree(path, ignore_errors=ignore_errors, **kwargs)

        with mock.patch.object(audit_dump.shutil, "rmtree", side_effect=rmtree), mock.patch.object(audit_dump, "convert") as convert, mock.patch.object(audit_dump.import_audit, "main", return_value=0) as audit:
            code, errors = self.run_main(self.dump, "--relinker", self.relinker, "--libs", "libs", "--work", work)
        self.assertEqual(code, 2)
        self.assertIn("cannot empty", errors)
        self.assertIn(str(work / "output"), errors)
        convert.assert_not_called()
        audit.assert_not_called()
        self.assertTrue(stale.exists())

    def test_output_directory_creation_failure_stops_before_conversion(self):
        work = self.root / "work"
        original_mkdir = Path.mkdir

        def mkdir(path, *args, **kwargs):
            if path == work / "output":
                raise PermissionError("permission denied")
            return original_mkdir(path, *args, **kwargs)

        with mock.patch.object(Path, "mkdir", mkdir), mock.patch.object(audit_dump, "prepare") as prepare, \
                mock.patch.object(audit_dump, "convert") as convert, mock.patch.object(audit_dump.import_audit, "main") as audit:
            code, errors = self.run_main(self.dump, "--relinker", self.relinker, "--libs", "libs", "--work", work)
        self.assertEqual(code, 2)
        self.assertIn("cannot create", errors)
        self.assertIn(str(work / "output"), errors)
        self.assertIn("permission denied", errors)
        prepare.assert_not_called()
        convert.assert_not_called()
        audit.assert_not_called()
        self.assertEqual((self.dump / "eboot.bin").read_bytes(), SELF)

    @unittest.skipIf(os.name == "nt" or (hasattr(os, "geteuid") and os.geteuid() == 0), "needs POSIX permissions and a non-root user")
    def test_a_non_writable_stale_directory_stops_before_conversion(self):
        work = self.root / "work"
        locked = work / "output" / "locked"
        locked.mkdir(parents=True)
        (locked / "stale.registry.json").write_text("[]")
        locked.chmod(0o500)
        try:
            with mock.patch.object(audit_dump, "convert") as convert, mock.patch.object(audit_dump.import_audit, "main", return_value=0) as audit:
                code, errors = self.run_main(self.dump, "--relinker", self.relinker, "--libs", "libs", "--work", work)
            self.assertEqual(code, 2)
            self.assertIn("cannot empty", errors)
            convert.assert_not_called()
            audit.assert_not_called()
        finally:
            locked.chmod(0o700)

    def test_a_generated_directory_that_survives_removal_stops_before_conversion(self):
        work = self.root / "work"
        (work / "source").mkdir(parents=True)
        with mock.patch.object(audit_dump.shutil, "rmtree"), mock.patch.object(audit_dump, "convert") as convert, mock.patch.object(audit_dump.import_audit, "main", return_value=0) as audit:
            code, errors = self.run_main(self.dump, "--relinker", self.relinker, "--libs", "libs", "--work", work)
        self.assertEqual(code, 2)
        self.assertIn("still exists", errors)
        convert.assert_not_called()
        audit.assert_not_called()

    def test_work_cleanup_refuses_relinker_or_libraries_in_generated_directories(self):
        for generated in ("source", "output"):
            for kind in ("relinker", "libs"):
                with self.subTest(generated=generated, kind=kind), tempfile.TemporaryDirectory() as directory:
                    work = Path(directory)
                    target = work / generated / "nested"
                    target.mkdir(parents=True)
                    marker = target / "keep.prx"
                    marker.write_bytes(b"keep")
                    relinker = marker if kind == "relinker" else self.relinker
                    libs = target if kind == "libs" else self.root / "libs"
                    with mock.patch.object(audit_dump, "prepare") as prepare, \
                            mock.patch.object(audit_dump, "convert") as convert, \
                            self.assertRaises(SystemExit) as error:
                        self.run_main(self.dump, "--relinker", relinker, "--libs", libs, "--work", work)
                    self.assertEqual(error.exception.code, 2)
                    self.assertEqual(marker.read_bytes(), b"keep")
                    prepare.assert_not_called()
                    convert.assert_not_called()

    def test_work_cleanup_refuses_outward_input_links(self):
        for generated in ("source", "output"):
            for kind in ("relinker", "libs"):
                with self.subTest(generated=generated, kind=kind), tempfile.TemporaryDirectory() as directory:
                    work = Path(directory)
                    nested = work / generated / "nested"
                    nested.mkdir(parents=True)
                    outside = self.relinker if kind == "relinker" else self.root
                    link = nested / "input-link"
                    try:
                        os.symlink(outside, link, target_is_directory=kind == "libs")
                    except OSError:
                        self.skipTest("symbolic links unavailable")
                    relinker = link if kind == "relinker" else self.relinker
                    libs = link if kind == "libs" else self.root
                    with mock.patch.object(audit_dump, "prepare") as prepare, \
                            mock.patch.object(audit_dump, "convert") as convert, \
                            self.assertRaises(SystemExit) as error:
                        self.run_main(self.dump, "--relinker", relinker, "--libs", libs, "--work", work)
                    self.assertEqual(error.exception.code, 2)
                    self.assertTrue(link.is_symlink())
                    prepare.assert_not_called()
                    convert.assert_not_called()

    def test_symbolic_link_loops_are_usage_errors_before_input_is_removed(self):
        loop = self.root / "loop"
        try:
            os.symlink(loop, loop)
        except OSError:
            self.skipTest("symbolic links unavailable")
        for name, arguments, expected in (
            ("work", ("--relinker", self.relinker, "--work", loop), "cannot check work directory against the input dump"),
            ("relinker", ("--relinker", loop), "no such relinker"),
            ("json", ("--relinker", self.relinker, "--json", loop), "cannot check JSON output against the input dump"),
            ("libs", ("--relinker", self.relinker, "--libs", loop), "cannot check work directory against input"),
        ):
            with self.subTest(name=name):
                errors = io.StringIO()
                with mock.patch.object(audit_dump, "prepare", return_value=([], [])) as prepare, mock.patch.object(audit_dump, "convert", return_value=subprocess.CompletedProcess([], 0)) as convert:
                    with contextlib.redirect_stderr(errors), self.assertRaises(SystemExit) as error:
                        audit_dump.main([str(argument) for argument in (self.dump, "--libs", "libs", *arguments)])
                self.assertEqual(error.exception.code, 2)
                self.assertIn(expected, errors.getvalue())
                prepare.assert_not_called()
                convert.assert_not_called()
                self.assertEqual((self.dump / "eboot.bin").read_bytes(), SELF)

    def assert_overlap_rejected(self, dump, work):
        marker = dump / "original-marker"
        marker.write_bytes(b"preserve original")
        original = (dump / "eboot.bin").read_bytes()
        with mock.patch.object(audit_dump, "prepare", return_value=([], [])) as prepare, mock.patch.object(audit_dump, "convert", return_value=subprocess.CompletedProcess([], 0)) as convert, mock.patch.object(audit_dump.import_audit, "main", return_value=0):
            with self.assertRaises(SystemExit) as error:
                self.run_main(dump, "--relinker", self.relinker, "--libs", "libs", "--work", work)
        self.assertEqual(error.exception.code, 2)
        prepare.assert_not_called()
        convert.assert_not_called()
        self.assertEqual((dump / "eboot.bin").read_bytes(), original)
        self.assertEqual(marker.read_bytes(), b"preserve original")

    def test_overlapping_work_paths_are_rejected_before_input_is_removed(self):
        for name, relative in (("source", "source"), ("output", "output"), ("nested-source", "source/nested"), ("nested-output", "output/nested"), ("work-is-dump", "."), ("work-in-dump", "ancestor")):
            with self.subTest(name=name):
                root = self.root / name
                dump = root / relative
                dump.mkdir(parents=True)
                (dump / "eboot.bin").write_bytes(SELF)
                work = dump / "work" if name == "work-in-dump" else root
                self.assert_overlap_rejected(dump, work)

    def assert_report_rejected(self, report):
        work = self.root / "report-work"
        marker = work / "source" / "preserve"
        marker.parent.mkdir(parents=True, exist_ok=True)
        marker.write_bytes(b"previous work")
        original = (self.dump / "eboot.bin").read_bytes()
        with mock.patch.object(audit_dump, "prepare", return_value=([], [])) as prepare, mock.patch.object(audit_dump, "convert", return_value=subprocess.CompletedProcess([], 0)) as convert, mock.patch.object(audit_dump.import_audit, "main", return_value=0) as audit:
            with self.assertRaises(SystemExit) as error:
                self.run_main(self.dump, "--relinker", self.relinker, "--libs", "libs", "--work", work, "--json", report)
        self.assertEqual(error.exception.code, 2)
        prepare.assert_not_called()
        convert.assert_not_called()
        audit.assert_not_called()
        self.assertEqual((self.dump / "eboot.bin").read_bytes(), original)
        self.assertEqual(marker.read_bytes(), b"previous work")

    def test_json_outputs_inside_the_dump_are_rejected_before_staging(self):
        for relative in (".", "eboot.bin", "report.json", "new/directory/report.json"):
            with self.subTest(relative=relative):
                self.assert_report_rejected(self.dump / relative)

    def test_json_output_symlink_aliases_of_the_dump_are_rejected(self):
        for name, target, directory in (("file", self.dump / "eboot.bin", False), ("directory", self.dump, True), ("missing", self.dump / "not-created.json", False)):
            with self.subTest(name=name):
                link = self.root / ("report-link-" + name)
                try:
                    os.symlink(target, link, target_is_directory=directory)
                except OSError:
                    self.skipTest("symbolic links unavailable")
                self.assert_report_rejected(link / "new.json" if directory else link)

    def test_windows_resolved_path_spellings_protect_missing_reports(self):
        for normal, extended in (
            (PureWindowsPath("C:/dump"), PureWindowsPath("//?/C:/dump")),
            (PureWindowsPath("//server/share/dump"), PureWindowsPath("//?/UNC/server/share/dump")),
        ):
            for report_root, dump_root in ((normal, extended), (extended, normal)):
                with self.subTest(report_root=report_root, dump_root=dump_root):
                    dump = mock.Mock()
                    dump.resolve.return_value = dump_root
                    dump.absolute.return_value = normal
                    dump.rglob.return_value = ()
                    dump.parents = ()
                    dump.is_symlink.return_value = False
                    report = mock.Mock()
                    report.resolve.return_value = report_root / "not-created.json"
                    report.absolute.return_value = normal.parent / "outside-report.json"
                    report.parent.resolve.return_value = normal.parent
                    report.parents = ()
                    report.is_symlink.return_value = False
                    report.parent.parents = ()
                    report.parent.is_symlink.return_value = False
                    report.exists.return_value = False
                    self.assertTrue(audit_dump.report_overlaps_dump(report, dump))
                    report.resolve.return_value = report_root.parent / "outside-report.json"
                    self.assertFalse(audit_dump.report_overlaps_dump(report, dump))

    def test_json_report_links_are_checked_when_resolve_leaves_the_alias(self):
        resolve = Path.resolve
        for name, target, directory in (
            ("file", self.dump / "missing-report.json", False),
            ("directory", self.dump / "missing-directory", True),
        ):
            with self.subTest(name=name):
                link = self.root / ("unresolved-report-" + name)
                try:
                    os.symlink(target, link, target_is_directory=directory)
                except OSError:
                    self.skipTest("symbolic links unavailable")
                report = link / "report.json" if directory else link

                def unresolved_alias(path, *args, **kwargs):
                    if path == link or path.is_relative_to(link):
                        return path.absolute()
                    return resolve(path, *args, **kwargs)

                with mock.patch.object(Path, "resolve", unresolved_alias):
                    self.assert_report_rejected(report)

    def test_json_report_link_cycles_are_rejected_before_staging(self):
        first = self.root / "report-cycle-first"
        second = self.root / "report-cycle-second"
        try:
            os.symlink(second, first)
            os.symlink(first, second)
        except OSError:
            self.skipTest("symbolic links unavailable")
        self.assert_report_rejected(first)

    def test_repeated_parent_alias_in_link_target_is_not_a_cycle(self):
        alias = self.root / "parent-alias"
        link = self.root / "report-through-parent"
        target = self.root / "external-report.json"
        try:
            os.symlink(self.root, alias, target_is_directory=True)
            os.symlink(alias / target.name, link)
        except OSError:
            self.skipTest("symbolic links unavailable")
        self.assertEqual(audit_dump.comparison_path(alias / link.name), target.resolve())
        self.assertFalse(audit_dump.report_overlaps_dump(alias / link.name, self.dump))

    def test_json_output_hardlink_aliases_of_dump_files_are_rejected(self):
        self.module("Media/Plugins/libfmod.prx")
        for name in ("eboot.bin", "Media/Plugins/libfmod.prx"):
            with self.subTest(name=name):
                link = self.root / (Path(name).name + ".report")
                try:
                    os.link(self.dump / name, link)
                except OSError:
                    self.skipTest("hard links unavailable")
                self.assert_report_rejected(link)

    def test_json_output_inside_the_dump_cannot_escape_through_a_symlink(self):
        report = self.root / "outside-report.json"
        report.write_bytes(b"preserve report")
        link = self.dump / "report.json"
        try:
            os.symlink(report, link)
        except OSError:
            self.skipTest("symbolic links unavailable")
        self.assert_report_rejected(link)
        self.assertEqual(report.read_bytes(), b"preserve report")

    def test_json_output_cannot_replace_files_in_a_linked_dump_directory(self):
        modules = self.root / "shared-modules"
        modules.mkdir()
        module = modules / "libc.prx"
        module.write_bytes(ELF)
        try:
            os.symlink(modules, self.dump / "sce_module", target_is_directory=True)
        except OSError:
            self.skipTest("symbolic links unavailable")
        for report in (module, modules / "new-report.json"):
            with self.subTest(report=report):
                self.assert_report_rejected(report)
                self.assertEqual(module.read_bytes(), ELF)
        self.assertFalse((modules / "new-report.json").exists())

    def test_a_dump_alias_still_protects_an_outward_report_link(self):
        original = self.dump
        alias = self.root / "dump-alias"
        report = original / "report.json"
        outside = self.root / "not-created.json"
        try:
            os.symlink(original, alias, target_is_directory=True)
            os.symlink(outside, report)
        except OSError:
            self.skipTest("symbolic links unavailable")
        self.dump = alias
        try:
            self.assert_report_rejected(report)
        finally:
            self.dump = original
        self.assertFalse(outside.exists())

    def test_symlinked_work_paths_are_checked_against_the_resolved_dump(self):
        for generated in ("source", "output"):
            with self.subTest(generated=generated):
                work = self.root / ("linked-" + generated)
                work.mkdir()
                try:
                    os.symlink(self.dump, work / generated, target_is_directory=True)
                except OSError:
                    self.skipTest("symbolic links unavailable")
                self.assert_overlap_rejected(self.dump, work)

    def test_a_broken_module_link_fails_instead_of_being_skipped(self):
        (self.dump / "sce_module").mkdir()
        try:
            os.symlink(self.dump / "sce_module" / "missing.prx", self.dump / "sce_module" / "libc.prx")
        except OSError:
            self.skipTest("symbolic links unavailable")
        with mock.patch.object(subprocess, "run") as run:
            code, errors = self.run_main(self.dump, "--relinker", self.relinker, "--libs", "libs")
        self.assertEqual(code, 2)
        self.assertIn("libc.prx", errors)
        run.assert_not_called()

    def test_a_relinker_failure_returns_2_with_its_message(self):
        with mock.patch.object(subprocess, "run", return_value=subprocess.CompletedProcess([], 3, "", "bad input")), mock.patch.object(audit_dump.import_audit, "main") as audit:
            code, errors = self.run_main(self.dump, "--relinker", self.relinker, "--libs", "libs")
        self.assertEqual(code, 2)
        self.assertIn("relinker exited with 3", errors)
        self.assertIn("bad input", errors)
        audit.assert_not_called()

    def test_relinker_launch_errors_return_2_without_auditing(self):
        for error in (PermissionError(13, "Permission denied", str(self.relinker)),
                      FileNotFoundError(2, "No such file or directory", str(self.relinker))):
            with self.subTest(error=type(error).__name__):
                with mock.patch.object(subprocess, "run", side_effect=error), mock.patch.object(audit_dump.import_audit, "main") as audit:
                    code, errors = self.run_main(self.dump, "--relinker", self.relinker, "--libs", "libs")
                self.assertEqual(code, 2)
                self.assertIn("cannot run relinker", errors)
                self.assertIn(str(self.relinker.resolve()), errors)
                self.assertIn(error.strerror, errors)
                audit.assert_not_called()
                self.assertEqual((self.dump / "eboot.bin").read_bytes(), SELF)

    def test_an_invalid_relinker_executable_is_reported_without_a_traceback(self):
        (self.dump / "eboot.bin").write_bytes(ELF)
        result = subprocess.run([sys.executable, audit_dump.__file__, str(self.dump),
                                 "--relinker", str(self.relinker), "--libs", str(self.root / "missing-libs")],
                                capture_output=True, text=True, encoding="utf-8",
                                env=dict(os.environ, PYTHONIOENCODING="utf-8"))
        self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
        self.assertIn("cannot run relinker", result.stderr)
        self.assertIn(str(self.relinker.resolve()), result.stderr)
        self.assertNotIn("Traceback", result.stderr)
        self.assertEqual((self.dump / "eboot.bin").read_bytes(), ELF)

    def test_a_dump_without_eboot_or_a_missing_relinker_is_an_argument_error(self):
        (self.dump / "eboot.bin").unlink()
        with self.assertRaises(SystemExit):
            self.run_main(self.dump, "--relinker", self.relinker, "--libs", "libs")
        (self.dump / "eboot.bin").write_bytes(SELF)
        with self.assertRaises(SystemExit):
            self.run_main(self.dump, "--relinker", self.root / "missing.exe", "--libs", "libs")


if __name__ == "__main__":
    unittest.main()
