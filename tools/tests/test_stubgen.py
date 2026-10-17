import contextlib
import io
import os
import stat
import tempfile
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import check_conventions
import stubgen


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

    def test_prx_suffix_is_stripped_from_module_name(self):
        nids = ["AAAAAAAAAAA", "BBBBBBBBBBB"]
        expected = stubgen.generate("libSceAmpr", nids, {})
        self.assertIn("sceAmprUnknown00", expected[0])
        self.assertIn("core/libs/prx/libSceAmpr/", expected[1])
        for spelling in ("libSceAmpr.prx", "libSceAmpr.PRX"):
            with self.subTest(spelling=spelling):
                self.assertEqual(stubgen.generate(spelling, nids, {}), expected)


class StubGeneratorTests(unittest.TestCase):
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
                    capture_output=True, text=True)
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


if __name__ == "__main__":
    unittest.main()
