import contextlib
import io
import os
import stat
import tempfile
from pathlib import Path
import sys
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import check_conventions
import stubgen
from nid_names import compute_nid


class StubgenTests(unittest.TestCase):
    def test_default_debt_link_resolves_from_the_library_debt_file(self):
        _, td = stubgen.generate("libkernel", ["AC1FtjqMCL0"], {})
        self.assertEqual(check_conventions.debt_name(td), "libkernel")
        found = check_conventions.links("docs/dev/debt/libkernel.md", td.strip())
        self.assertTrue(found)
        for item in found:
            self.assertEqual(item[1], "core/libs/prx/libkernel/Unimplemented.cpp")

    def test_explicit_td_path_is_verbatim(self):
        _, td = stubgen.generate("libkernel", ["AC1FtjqMCL0"], {}, td_path="x/y.cpp")
        self.assertIn("(x/y.cpp)", td)

    def test_code_still_generated(self):
        code, _ = stubgen.generate("libkernel", ["AC1FtjqMCL0"], {})
        self.assertIn("APS5_VABI", code)
        self.assertIn("NotImplemented_nid_no_patch", code)

    def test_unknown_exports_cannot_consume_verified_known_names(self):
        known_name = "sceAmprUnknown00"
        known_nid = compute_nid(known_name)
        unknown_nid = "AAAAAAAAAAA"
        for order in ((unknown_nid, known_nid), (known_nid, unknown_nid)):
            with self.subTest(order=order):
                code, debt = stubgen.generate("libSceAmpr", order, {known_nid: known_name})
                self.assertIn("int APS5_VABI sceAmprUnknown00(void)", code)
                self.assertIn('APS5_EXPORT("AAAAAAAAAAA", sceAmprUnknown01);', code)
                self.assertIn("int APS5_VABI sceAmprUnknown01(void)", code)
                self.assertIn("[sceAmprUnknown00]", debt)
                self.assertIn("[AAAAAAAAAAA]", debt)
                self.assertNotIn(f'APS5_EXPORT("{known_nid}", sceAmprUnknown', code)

    def test_generate_preserves_single_pass_iterable_inputs(self):
        known = "6OkOa1SOKuY"
        db = {known: "sceAmprUnknown00"}
        for values in (("AAAAAAAAAAA", known), (known, "AAAAAAAAAAA")):
            expected = stubgen.generate("libSceAmpr", values, db)
            for label, factory in (("iterator", iter), ("generator", lambda items: (nid for nid in items))):
                with self.subTest(values=values, input=label):
                    self.assertEqual(stubgen.generate("libSceAmpr", factory(values), db), expected)

    def test_prx_suffix_is_stripped_from_module_name(self):
        nids = ["AAAAAAAAAAA", "BBBBBBBBBBB"]
        expected = stubgen.generate("libSceAmpr", nids, {})
        self.assertIn("sceAmprUnknown00", expected[0])
        self.assertIn("core/libs/prx/libSceAmpr/", expected[1])
        for spelling in ("libSceAmpr.prx", "libSceAmpr.PRX"):
            with self.subTest(spelling=spelling):
                self.assertEqual(stubgen.generate(spelling, nids, {}), expected)


class StubGeneratorTests(unittest.TestCase):
    def test_long_output_names(self):
        import subprocess
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory).resolve()
            if os.name == "nt":
                root = Path("\\\\?\\" + str(root))
            limit = os.pathconf(root, "PC_NAME_MAX") if os.name == "posix" else 255
            nids = root / "nids.txt"
            nids.write_text("AAAAAAAAAAA\n", encoding="utf-8")
            expected = stubgen.generate("libSceExample", ["AAAAAAAAAAA"], {})
            for option, extra, content in (("--out", [], expected[0]),
                                           ("--td-out", ["--td-only"], expected[1])):
                for existing in (False, True):
                    with self.subTest(option=option, existing=existing):
                        output = root / ("a" * (limit - 4) + ".txt")
                        output.write_bytes(b"previous output\n")
                        if not existing:
                            output.unlink()
                        result = subprocess.run(
                            [sys.executable, str(Path(stubgen.__file__)), "--module", "libSceExample",
                             "--nids", str(nids), option, str(output), *extra],
                            capture_output=True, text=True)
                        self.assertEqual(result.returncode, 0, result.stderr)
                        self.assertEqual(output.read_text(encoding="utf-8"), content)
                        self.assertEqual(nids.read_text(encoding="utf-8"), "AAAAAAAAAAA\n")
                        self.assertEqual(set(root.iterdir()), {nids, output})
                        output.unlink()

    @unittest.skipUnless(os.name == "nt", "Windows denies replacing read-only output files")
    def test_readonly_second_output_preserves_both_outputs(self):
        import subprocess
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            nids = root / "nids.txt"
            code = root / "Export.cpp"
            debt = root / "debt.md"
            nids.write_text("AC1FtjqMCL0\n", encoding="utf-8")
            code.write_bytes(b"original code\n")
            debt.write_bytes(b"original debt\n")
            debt.chmod(stat.S_IREAD)
            try:
                result = subprocess.run(
                    [sys.executable, str(Path(stubgen.__file__)), "--module", "libSceTest",
                     "--nids", str(nids), "--out", str(code), "--td-out", str(debt)],
                    capture_output=True, encoding="utf-8")
                self.assertNotEqual(result.returncode, 0)
                self.assertNotIn("Traceback", result.stderr)
                self.assertEqual(code.read_bytes(), b"original code\n")
                self.assertEqual(debt.read_bytes(), b"original debt\n")
                self.assertEqual(sorted(path.name for path in root.iterdir()), ["Export.cpp", "debt.md", "nids.txt"])
            finally:
                debt.chmod(stat.S_IREAD | stat.S_IWRITE)

    def test_default_stdout_contains_only_cpp(self):
        with tempfile.TemporaryDirectory() as directory:
            nids = Path(directory) / "nids.txt"
            nids.write_text("AAAAAAAAAAA\n", encoding="utf-8")
            output, debt = io.StringIO(), io.StringIO()
            with contextlib.redirect_stdout(output), contextlib.redirect_stderr(debt):
                self.assertEqual(stubgen.main(["--module", "libSceExample", "--nids", str(nids)]), 0)
            self.assertTrue(output.getvalue().endswith("\n}\n"))
            self.assertNotIn("unknown name, signature", output.getvalue())
            self.assertIn("unknown name, signature", debt.getvalue())

    def test_output_cannot_overwrite_an_input_file(self):
        with tempfile.TemporaryDirectory() as directory:
            nids = Path(directory) / "nids.txt"
            original = "AAAAAAAAAAA\n"
            nids.write_text(original, encoding="utf-8")
            with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit) as error:
                stubgen.main(["--module", "libSceExample", "--nids", str(nids), "--out", str(nids)])
            self.assertEqual(error.exception.code, 2)
            self.assertEqual(nids.read_text(encoding="utf-8"), original)

    def test_debt_write_failure_preserves_existing_code(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            nids, output = root / "nids.txt", root / "Export.cpp"
            nids.write_text("AAAAAAAAAAA\n", encoding="utf-8")
            output.write_text("existing source\n", encoding="utf-8")
            arguments = ["--module", "libSceExample", "--nids", str(nids), "--out", str(output), "--td-out", str(root / "absent" / "debt.txt")]
            with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit) as error:
                stubgen.main(arguments)
            self.assertEqual(error.exception.code, 2)
            self.assertEqual(output.read_text(encoding="utf-8"), "existing source\n")
            self.assertEqual(sorted(p.name for p in root.iterdir()), ["Export.cpp", "nids.txt"])

    def test_hard_linked_database_output_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            nids, database, output = root / "nids.txt", root / "names.txt", root / "Export.cpp"
            nids.write_text("AAAAAAAAAAA\n", encoding="utf-8")
            original = "AAAAAAAAAAA sceExample\n"
            database.write_text(original, encoding="utf-8")
            os.link(database, output)
            arguments = ["--module", "libSceExample", "--nids", str(nids), "--db", str(database), "--out", str(output)]
            with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit) as error:
                stubgen.main(arguments)
            self.assertEqual(error.exception.code, 2)
            self.assertEqual(database.read_text(encoding="utf-8"), original)
            self.assertEqual(output.read_text(encoding="utf-8"), original)

    def test_malformed_nid_is_rejected_before_generating_cpp(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "nids.txt"
            path.write_text('AAAAAAAAAAA\ninvalid"nid\n', encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "invalid NID"):
                stubgen.parse_nids(path)

    def test_malformed_database_rows_are_rejected_with_location(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "names.txt"
            cases = {
                "missing name": "AAAAAAAAAAA\n",
                "short NID": "AAAAAAAAAA sceShort\n",
                "invalid NID": 'AAAAAAAA"AA sceQuoted\n',
                "blank name after comment": "AAAAAAAAAAA # sceCommented\n",
            }
            for label, content in cases.items():
                with self.subTest(label=label):
                    path.write_text(content, encoding="utf-8")
                    with self.assertRaisesRegex(ValueError, r"names\.txt:1:"):
                        stubgen.parse_db(path)

    def test_valid_database_rows_are_parsed(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "names.txt"
            path.write_text("# comment\nAAAAAAAAAAA sceGood\n"
                            "BBBBBBBBBBB sceOther # trailing note\n\n"
                            "+CCCCCCCCCC scePlus\n", encoding="utf-8")
            self.assertEqual(stubgen.parse_db(path),
                             {"AAAAAAAAAAA": "sceGood", "BBBBBBBBBBB": "sceOther", "+CCCCCCCCCC": "scePlus"})

    def test_malformed_database_reaches_the_cli_as_an_error(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            nids, database = root / "nids.txt", root / "names.txt"
            nids.write_text("AAAAAAAAAAA\n", encoding="utf-8")
            database.write_text("AAAAAAAAAA sceShort\n", encoding="utf-8")
            arguments = ["--module", "libSceExample", "--nids", str(nids), "--db", str(database)]
            with contextlib.redirect_stderr(io.StringIO()) as output, \
                    contextlib.redirect_stdout(io.StringIO()), self.assertRaises(SystemExit) as error:
                stubgen.main(arguments)
            self.assertEqual(error.exception.code, 2)
            self.assertIn("invalid NID", output.getvalue())

    def test_code_and_debt_outputs_cannot_overwrite_each_other(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            nids = root / "nids.txt"
            nids.write_text("AAAAAAAAAAA\n", encoding="utf-8")
            output = root / "Export.cpp"
            arguments = ["--module", "libSceExample", "--nids", str(nids), "--out", str(output), "--td-out", str(output)]
            with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit) as error:
                stubgen.main(arguments)
            self.assertEqual(error.exception.code, 2)
            self.assertFalse(output.exists())

    def test_valid_annotations_comments_and_duplicates_are_preserved(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "nids.txt"
            path.write_text("# imports\nAAAAAAAAAAA#A#B\nAAAAAAAAAAA\nBBBBBBBBBBB\n", encoding="utf-8-sig")
            self.assertEqual(stubgen.parse_nids(path), ["AAAAAAAAAAA", "BBBBBBBBBBB"])

    def test_named_stub_reports_its_function_name(self):
        code = stubgen.generate("libSceAmpr", ["tNn5WBkta60", "AAAAAAAAAAA"], {"tNn5WBkta60": "sceAmprCommandBufferNop"})[0]
        named = code.split("sceAmprCommandBufferNop(void) {\n", 1)[1].split("\n", 1)[0]
        self.assertEqual(named, "    NotImplemented_nid_no_patch(__func__);")
        self.assertIn('NotImplemented_nid_no_patch("AAAAAAAAAAA");', code)

    def test_valid_generation_keeps_the_code_and_debt(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            nids = root / "nids.txt"
            nids.write_text("AAAAAAAAAAA\n", encoding="utf-8")
            code, debt = root / "Export.cpp", root / "debt.txt"
            arguments = ["--module", "libSceExample", "--nids", str(nids), "--out", str(code), "--td-out", str(debt)]
            self.assertEqual(stubgen.main(arguments), 0)
            self.assertIn('APS5_EXPORT("AAAAAAAAAAA",', code.read_text(encoding="utf-8"))
            self.assertIn("unknown name, signature", debt.read_text(encoding="utf-8"))
            if sys.platform != "win32":
                mask = os.umask(0o022)
                os.umask(mask)
                self.assertEqual(code.stat().st_mode & 0o777, 0o666 & ~mask)


class ReadOnlyOutputTests(unittest.TestCase):
    def test_later_staging_failure_preserves_original_error_and_cleans_readonly_temporary(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            nids, output = root / "nids.txt", root / "Export.cpp"
            nids.write_text("AAAAAAAAAAA\n", encoding="utf-8")
            original = "existing source\n"
            output.write_text(original, encoding="utf-8")
            output.chmod(0o444)
            errors = io.StringIO()
            arguments = ["--module", "libSceExample", "--nids", str(nids), "--out", str(output),
                         "--td-out", str(root / "absent" / "debt.txt")]
            with contextlib.redirect_stderr(errors), self.assertRaises(SystemExit) as error:
                stubgen.main(arguments)
            self.assertEqual(error.exception.code, 2)
            self.assertIn("absent", errors.getvalue())
            self.assertNotIn("Export.cpp.", errors.getvalue())
            self.assertEqual(output.read_text(encoding="utf-8"), original)
            self.assertFalse(output.stat().st_mode & stat.S_IWRITE)
            self.assertEqual(sorted(path.name for path in root.iterdir()), ["Export.cpp", "nids.txt"])

    @unittest.skipUnless(os.name == "nt", "Windows denies replacing read-only output files")
    def test_denied_replace_preserves_readonly_output_without_leaking_temporary(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            nids, output = root / "nids.txt", root / "Export.cpp"
            nids.write_text("AAAAAAAAAAA\n", encoding="utf-8")
            original = "existing source\n"
            output.write_text(original, encoding="utf-8")
            output.chmod(0o444)
            with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit) as error:
                stubgen.main(["--module", "libSceExample", "--nids", str(nids), "--out", str(output)])
            self.assertEqual(error.exception.code, 2)
            self.assertEqual(output.read_text(encoding="utf-8"), original)
            self.assertFalse(output.stat().st_mode & stat.S_IWRITE)
            self.assertEqual(sorted(path.name for path in root.iterdir()), ["Export.cpp", "nids.txt"])


    def test_main_reconfigures_streams(self):
        class Stream:
            def __init__(self):
                self.encoding = None
                self.errors = None
                self.newline = None

            def reconfigure(self, encoding=None, errors=None, newline=None):
                self.encoding = encoding
                self.errors = errors
                self.newline = newline

            def write(self, text):
                pass

            def flush(self):
                pass

        out, err = Stream(), Stream()
        with patch.object(stubgen.sys, "stdout", out), patch.object(stubgen.sys, "stderr", err):
            try:
                stubgen.main([])
            except SystemExit:
                pass
        self.assertEqual(out.encoding, "utf-8")
        self.assertEqual(out.errors, "backslashreplace")
        self.assertEqual(out.newline, "\n")
        self.assertEqual(err.encoding, "utf-8")
        self.assertEqual(err.errors, "backslashreplace")
        self.assertEqual(err.newline, "\n")


class CaseVariantOutputTests(unittest.TestCase):
    def test_case_variant_outputs_are_refused_before_writing(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            if not stubgen.case_insensitive_directory(root):
                self.skipTest("the output directory is case-sensitive")
            nids = root / "nids.txt"
            nids.write_text("AAAAAAAAAAA\n", encoding="utf-8")
            cases = [["--module", "libSceExample", "--nids", str(nids), "--out", str(root / "REPORT.TXT"),
                      "--td-out", str(root / "report.txt")]]
            try:
                link = root / "code-link.txt"
                link.symlink_to(root / "REPORT.TXT")
            except OSError:
                link = None
            if link is not None:
                cases.append(["--module", "libSceExample", "--nids", str(nids), "--out", str(link),
                              "--td-out", str(root / "report.txt")])
            for arguments in cases:
                with self.subTest(arguments=arguments):
                    with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit) as error:
                        stubgen.main(arguments)
                    self.assertEqual(error.exception.code, 2)
                    self.assertFalse((root / "REPORT.TXT").exists())
                    self.assertFalse((root / "report.txt").exists())

    def test_case_variant_output_with_an_existing_file_stays_refused(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            if not stubgen.case_insensitive_directory(root):
                self.skipTest("the output directory is case-sensitive")
            nids = root / "nids.txt"
            nids.write_text("AAAAAAAAAAA\n", encoding="utf-8")
            debt = root / "report.txt"
            original = "existing debt\n"
            debt.write_text(original, encoding="utf-8")
            arguments = ["--module", "libSceExample", "--nids", str(nids), "--out", str(root / "REPORT.TXT"),
                         "--td-out", str(debt)]
            with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit) as error:
                stubgen.main(arguments)
            self.assertEqual(error.exception.code, 2)
            self.assertEqual(debt.read_text(encoding="utf-8"), original)

    def test_windows_link_targets_drop_the_extended_length_prefix(self):
        with patch.object(stubgen.os, "name", "nt"):
            self.assertEqual(stubgen.link_target("\\\\?\\C:\\out\\REPORT.TXT"), "C:\\out\\REPORT.TXT")
            self.assertEqual(stubgen.link_target("\\\\?\\UNC\\host\\share\\a.txt"), "\\\\host\\share\\a.txt")
            self.assertEqual(stubgen.link_target("..\\REPORT.TXT"), "..\\REPORT.TXT")
        with patch.object(stubgen.os, "name", "posix"):
            self.assertEqual(stubgen.link_target("\\\\?\\C:\\x"), "\\\\?\\C:\\x")

    def test_case_variant_outputs_stay_distinct_on_case_sensitive_directories(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            missing = root / "MiXeD.CaSe"
            if missing.resolve().name != missing.name:
                self.skipTest("Path.resolve() normalizes the case of missing files")
            upper, lower = root / "REPORT.TXT", root / "report.txt"
            try:
                link = root / "code-link.txt"
                link.symlink_to(upper)
            except OSError:
                link = None
            with patch.object(stubgen, "case_insensitive_directory", return_value=False):
                self.assertFalse(stubgen.same_file(upper, lower))
                if link is not None:
                    self.assertFalse(stubgen.same_file(link, lower))
            if stubgen.case_insensitive_directory(root):
                self.skipTest("the output directory is case-insensitive")
            nids = root / "nids.txt"
            nids.write_text("AAAAAAAAAAA\n", encoding="utf-8")
            arguments = ["--module", "libSceExample", "--nids", str(nids), "--out", str(upper),
                         "--td-out", str(lower)]
            self.assertEqual(stubgen.main(arguments), 0)
            self.assertIn('APS5_EXPORT("AAAAAAAAAAA",', upper.read_text(encoding="utf-8"))
            self.assertIn("unknown name, signature", lower.read_text(encoding="utf-8"))


class SymlinkLoopTests(unittest.TestCase):
    def test_looping_paths_are_usage_errors(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            first, second = root / "first", root / "second"
            try:
                first.symlink_to(second)
                second.symlink_to(first)
            except OSError as error:
                self.skipTest(f"symbolic links are unavailable: {error}")
            nids = root / "nids.txt"
            nids.write_text("AAAAAAAAAAA\n", encoding="utf-8")
            cases = {
                "out": ["--nids", str(nids), "--out", str(first)],
                "td-out": ["--nids", str(nids), "--out", str(root / "code.cpp"), "--td-out", str(first)],
                "nids": ["--nids", str(first), "--out", str(root / "code.cpp")],
                "parent": ["--nids", str(nids), "--out", str(first / "code.cpp")],
            }
            for name, arguments in cases.items():
                with self.subTest(name), contextlib.redirect_stderr(io.StringIO()) as errors, \
                        self.assertRaises(SystemExit) as error:
                    stubgen.main(["--module", "libSceExample", *arguments])
                self.assertEqual(error.exception.code, 2)
                if name == "parent":
                    self.assertTrue(any(repr(str(link))[1:-1] in errors.getvalue() for link in (first, second)), errors.getvalue())
                else:
                    self.assertIn(repr(str(first))[1:-1], errors.getvalue())
                    self.assertIn("Too many levels of symbolic links", errors.getvalue())
                self.assertTrue(first.is_symlink())
                self.assertTrue(second.is_symlink())
            self.assertFalse((root / "code.cpp").exists())


if __name__ == "__main__":
    unittest.main()
