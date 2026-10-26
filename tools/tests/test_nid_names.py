import contextlib
import io
import json
import os
import subprocess
import tempfile
import sys
import unittest
from pathlib import Path, PureWindowsPath
from unittest.mock import patch
from urllib.error import URLError

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import nid_names


class DatabaseCacheTests(unittest.TestCase):
    def long_cache_path(self, directory):
        root = Path(directory)
        if os.name == "nt":
            root = Path("\\\\?\\" + str(root.resolve()))
        limit = os.pathconf(root, "PC_NAME_MAX") if os.name == "posix" else 255
        cache = root / ("n" * (limit - len(".csv")) + ".csv")
        cache.touch()
        cache.unlink()
        return cache

    def test_valid_long_cache_filename_can_be_downloaded(self):
        with tempfile.TemporaryDirectory() as directory:
            cache = self.long_cache_path(directory)

            def complete(url, filename):
                self.assertEqual(url, nid_names.DB_URL)
                self.assertEqual(Path(filename).parent, cache.parent)
                self.assertFalse(cache.exists())
                Path(filename).write_text("nid1 sceComplete\n", encoding="utf-8")

            try:
                with patch.object(nid_names.urllib.request, "urlretrieve", side_effect=complete) as download:
                    self.assertEqual(nid_names.load_db(cache), {"nid1": "sceComplete"})
                    download.assert_called_once()
                self.assertEqual(list(cache.parent.iterdir()), [cache])
                with patch.object(nid_names.urllib.request, "urlretrieve") as download:
                    self.assertEqual(nid_names.load_db(cache), {"nid1": "sceComplete"})
                    download.assert_not_called()
            finally:
                cache.unlink(missing_ok=True)

    def test_long_cache_filename_preserves_interruption_cleanup_and_retry(self):
        with tempfile.TemporaryDirectory() as directory:
            cache = self.long_cache_path(directory)

            def interrupted(url, filename):
                Path(filename).write_text("nid1 scePartial\n", encoding="utf-8")
                raise URLError("connection interrupted")

            def complete(url, filename):
                Path(filename).write_text("nid1 sceComplete\n", encoding="utf-8")

            try:
                with patch.object(nid_names.urllib.request, "urlretrieve", side_effect=interrupted):
                    with self.assertRaises(URLError):
                        nid_names.load_db(cache)
                self.assertFalse(cache.exists())
                self.assertEqual(list(cache.parent.iterdir()), [])
                with patch.object(nid_names.urllib.request, "urlretrieve", side_effect=complete):
                    self.assertEqual(nid_names.load_db(cache), {"nid1": "sceComplete"})
                self.assertEqual(list(cache.parent.iterdir()), [cache])
            finally:
                cache.unlink(missing_ok=True)

    def test_empty_symbol_filters_do_not_open_database(self):
        with tempfile.TemporaryDirectory() as directory:
            for option in ("--name", "--symbol", "--na", "--sym"):
                for json_output in (False, True):
                    with self.subTest(option=option, json_output=json_output):
                        result = subprocess.run(
                            [sys.executable, str(Path(nid_names.__file__)), option, "--db", directory,
                             *(["--json"] if json_output else [])], capture_output=True, text=True)
                        self.assertEqual(result.returncode, 0, result.stderr)
                        self.assertEqual(result.stderr, "")
                        if json_output:
                            self.assertEqual(json.loads(result.stdout), {"symbols": []})
                        else:
                            self.assertEqual(result.stdout.split(), ["NAME", "NID"])

    def test_empty_nid_filters_do_not_open_database(self):
        with tempfile.TemporaryDirectory() as directory:
            for option in ("--nid", "--ni"):
                for json_output in (False, True):
                    with self.subTest(option=option, json_output=json_output):
                        result = subprocess.run(
                            [sys.executable, str(Path(nid_names.__file__)), option, "--db", directory,
                             *(["--json"] if json_output else [])], capture_output=True, text=True, timeout=5)
                        self.assertEqual(result.returncode, 0, result.stderr)
                        self.assertEqual(result.stderr, "")
                        if json_output:
                            self.assertEqual(json.loads(result.stdout), {"db_entries": 0, "rows": []})
                        else:
                            self.assertIn("0 unknowns, 0 suggestions (0 already implemented)", result.stdout)

    def test_environment_variable_selects_the_database_and_help_names_it(self):
        script = str(Path(nid_names.__file__))
        helped = subprocess.run([sys.executable, script, "--help"], capture_output=True, text=True, timeout=60)
        self.assertEqual(helped.returncode, 0, helped.stderr)
        self.assertIn("ANYPS5_NID_DB", " ".join(helped.stdout.split()))
        with tempfile.TemporaryDirectory() as directory:
            database = Path(directory) / "nid database.csv"
            nid = nid_names.compute_nid("sceExample")
            database.write_text(f"{nid} sceExample\n", encoding="utf-8")
            scratch = str(Path(directory) / "scratch")
            Path(scratch).mkdir()
            tree = Path(directory) / "tree"
            (tree / "core" / "libs" / "prx").mkdir(parents=True)
            (tree / "tools").mkdir()
            tool = tree / "tools" / "nid_names.py"
            tool.write_bytes(Path(script).read_bytes())
            environment = {**os.environ, "ANYPS5_NID_DB": str(database), "TEMP": scratch, "TMP": scratch, "TMPDIR": scratch}
            result = subprocess.run([sys.executable, str(tool), "--nid", nid, "--json"], capture_output=True, text=True,
                                    env=environment, timeout=20)
            self.assertEqual(list(Path(scratch).iterdir()), [])
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertNotIn("downloading", result.stderr)
        self.assertIn("sceExample", result.stdout)

    def test_interrupted_download_does_not_publish_a_partial_cache(self):
        with tempfile.TemporaryDirectory() as directory:
            cache = Path(directory) / "cache" / "aerolib.csv"

            def interrupted(url, filename):
                Path(filename).write_text("nid1 scePartial\n", encoding="utf-8")
                raise URLError("connection interrupted")

            with patch.object(nid_names.urllib.request, "urlretrieve", side_effect=interrupted):
                with self.assertRaises(URLError):
                    nid_names.load_db(cache)
            self.assertFalse(cache.exists())
            self.assertEqual(list(cache.parent.iterdir()), [])

    def test_retry_after_interruption_downloads_the_complete_database(self):
        with tempfile.TemporaryDirectory() as directory:
            cache = Path(directory) / "aerolib.csv"

            def interrupted(url, filename):
                Path(filename).write_text("nid1 scePartial\n", encoding="utf-8")
                raise URLError("connection interrupted")

            def complete(url, filename):
                Path(filename).write_text("nid1 sceComplete\nnid2 sceOther\n", encoding="utf-8")

            with patch.object(nid_names.urllib.request, "urlretrieve", side_effect=interrupted):
                with self.assertRaises(URLError):
                    nid_names.load_db(cache)
            with patch.object(nid_names.urllib.request, "urlretrieve", side_effect=complete) as download:
                self.assertEqual(nid_names.load_db(cache), {"nid1": "sceComplete", "nid2": "sceOther"})
                download.assert_called_once()
            self.assertEqual(list(cache.parent.iterdir()), [cache])

    def test_cache_is_published_only_after_download_completes(self):
        with tempfile.TemporaryDirectory() as directory:
            cache = Path(directory) / "aerolib.csv"

            def complete(url, filename):
                self.assertEqual(url, nid_names.DB_URL)
                self.assertNotEqual(Path(filename), cache)
                self.assertEqual(Path(filename).parent, cache.parent)
                self.assertFalse(cache.exists())
                Path(filename).write_text("nid1 sceComplete\n", encoding="utf-8")

            with patch.object(nid_names.urllib.request, "urlretrieve", side_effect=complete):
                self.assertEqual(nid_names.load_db(cache), {"nid1": "sceComplete"})
            self.assertEqual(cache.read_text(encoding="utf-8"), "nid1 sceComplete\n")
            self.assertEqual(list(cache.parent.iterdir()), [cache])

    def test_existing_cache_is_used_without_downloading(self):
        with tempfile.TemporaryDirectory() as directory:
            cache = Path(directory) / "aerolib.csv"
            cache.write_text("nid1 sceExisting\n", encoding="utf-8")
            with patch.object(nid_names.urllib.request, "urlretrieve") as download:
                self.assertEqual(nid_names.load_db(cache), {"nid1": "sceExisting"})
                download.assert_not_called()

    def test_utf8_bom_does_not_change_the_first_nid(self):
        with tempfile.TemporaryDirectory() as directory:
            cache = Path(directory) / "aerolib.csv"
            nid = nid_names.compute_nid("sceExample")
            cache.write_text(f"{nid} sceExample\n", encoding="utf-8-sig")
            self.assertEqual(nid_names.load_db(cache), {nid: "sceExample"})

    def test_comment_rows_do_not_become_database_entries(self):
        with tempfile.TemporaryDirectory() as directory:
            cache = Path(directory) / "aerolib.csv"
            nid = nid_names.compute_nid("sceExample")
            cache.write_text(f"# Database names\n{nid} sceExample\n", encoding="utf-8")
            self.assertEqual(nid_names.load_db(cache), {nid: "sceExample"})

    def test_annotations_do_not_change_hash_inputs(self):
        with tempfile.TemporaryDirectory() as directory:
            cache = Path(directory) / "aerolib.csv"
            nid = nid_names.compute_nid("sceExample")
            cache.write_text(f"{nid}#A#B sceExample#verified\n", encoding="utf-8")
            self.assertEqual(nid_names.load_db(cache), {nid: "sceExample"})

class NidArgumentTests(unittest.TestCase):
    options = ["-h", "--help", "--db", "--nid", "--json"]

    def test_name_and_nid_modes_conflict_even_with_empty_filters(self):
        for name_option in ("--name", "--symbol", "--na", "--sym"):
            for nid_option in ("--nid", "--ni"):
                for names, nids in (
                        ([name_option], [nid_option]),
                        ([name_option, "ceilf"], [nid_option]),
                        ([name_option + "=ceilf"], [nid_option]),
                        ([name_option], [nid_option, "AAAAAAAAAAA"]),
                        ([name_option], [nid_option + "=AAAAAAAAAAA"])):
                    for arguments in (names + nids, nids + names):
                        with self.subTest(arguments=arguments), \
                                contextlib.redirect_stdout(io.StringIO()) as output, \
                                contextlib.redirect_stderr(io.StringIO()) as error, \
                                patch.object(nid_names, "load_db", return_value={}) as database, \
                                patch.object(nid_names, "collect_unknowns", return_value={}) as sources, \
                                patch.object(nid_names, "collect_real_names", return_value=set()) as definitions:
                            with self.assertRaises(SystemExit) as exited:
                                nid_names.main([*arguments, "--json"])
                            self.assertEqual(exited.exception.code, 2)
                            self.assertIn("--name/--symbol cannot be combined with --nid", error.getvalue())
                            self.assertEqual(output.getvalue(), "")
                            database.assert_not_called()
                            sources.assert_not_called()
                            definitions.assert_not_called()

    def test_invalid_nids_fail_before_database_or_source_access(self):
        for nid in ("short", "ABCDEFGHIJKL", "ABCDEFGHIJ/", "ABCDEFGHIJ_", "ABCDEFGHIJ\u00e9", "", "--jsooon"):
            with self.subTest(nid=nid):
                with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()) as error, \
                        patch.object(nid_names, "load_db", return_value={}) as database, \
                        patch.object(nid_names, "collect_unknowns") as sources, \
                        patch.object(nid_names, "collect_real_names", return_value=set()), \
                        self.assertRaises(SystemExit) as exited:
                    nid_names.main(["--nid=" + nid, "--json"])
                self.assertEqual(exited.exception.code, 2)
                self.assertIn("invalid NID", error.getvalue())
                database.assert_not_called()
                sources.assert_not_called()

    def test_option_typo_in_nid_list_is_not_an_unknown_success(self):
        with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()) as error, \
                patch.object(nid_names, "load_db", return_value={}) as database, \
                patch.object(nid_names, "collect_real_names", return_value=set()), self.assertRaises(SystemExit) as exited:
            nid_names.main(["--nid", "ZjtRqSMJwdw", "--jsooon"])
        self.assertEqual(exited.exception.code, 2)
        self.assertIn("--jsooon", error.getvalue())
        database.assert_not_called()

    def test_valid_unknown_nid_remains_a_successful_lookup(self):
        output = io.StringIO()
        with patch.object(nid_names, "load_db", return_value={}), \
                patch.object(nid_names, "collect_real_names", return_value=set()), \
                contextlib.redirect_stdout(output):
            self.assertEqual(nid_names.main(["--nid=AAAAAAAAAA+", "--json"]), 0)
        row, = json.loads(output.getvalue())["rows"]
        self.assertEqual((row["nid"], row["status"]), ("AAAAAAAAAA+", "unknown"))

    def test_nid_with_library_module_suffix_matches_the_bare_nid(self):
        results = []
        for nid in ("ZjtRqSMJwdw", "ZjtRqSMJwdw#C#D"):
            output = io.StringIO()
            with patch.object(nid_names, "load_db", return_value={"ZjtRqSMJwdw": "sinh"}), \
                    patch.object(nid_names, "collect_real_names", return_value=set()), \
                    contextlib.redirect_stdout(output):
                self.assertEqual(nid_names.main(["--nid=" + nid, "--json"]), 0)
            results.append(json.loads(output.getvalue())["rows"])
        self.assertEqual(results[0], results[1])
        self.assertEqual(results[1][0]["nid"], "ZjtRqSMJwdw")

    def normalize(self, arguments):
        return nid_names.nid_arguments(arguments, self.options)

    def test_nids_starting_with_a_dash_are_accepted(self):
        self.assertEqual(
            self.normalize(["--nid", "-nvxBWa0iDs", "ZjtRqSMJwdw", "--ll-AgiK2M", "--json"]),
            ["--nid=-nvxBWa0iDs", "--nid=ZjtRqSMJwdw", "--nid=--ll-AgiK2M", "--json"])

    def test_nid_with_value_after_a_bare_nid_is_kept(self):
        self.assertEqual(self.normalize(["--nid", "A", "--nid=B"]), ["--nid=A", "--nid=B"])

    def test_abbreviated_options_end_the_nids(self):
        self.assertEqual(self.normalize(["--nid", "A", "--j"]), ["--nid=A", "--j"])
        self.assertEqual(self.normalize(["--nid", "A", "--d=x.csv"]), ["--nid=A", "--d=x.csv"])
        self.assertEqual(self.normalize(["--ni", "-nvxBWa0iDs"]), ["--nid=-nvxBWa0iDs"])

    def test_end_of_options_marker_is_kept(self):
        self.assertEqual(self.normalize(["--nid", "A", "--", "B"]), ["--nid=A", "--", "B"])

    def test_main_resolves_every_nid_given_after_nid(self):
        database = {"-nvxBWa0iDs": "gethostname", "ZjtRqSMJwdw": "sinh"}
        arguments = ["nid_names.py", "--json", "--nid", "-nvxBWa0iDs", "ZjtRqSMJwdw"]
        with (patch.object(sys, "argv", arguments),
              patch.object(nid_names, "load_db", return_value=database),
              patch.object(nid_names, "collect_real_names", return_value=set()),
              patch("builtins.print") as printed):
            self.assertEqual(nid_names.main(), 0)
        rows = json.loads(printed.call_args.args[0])["rows"]
        self.assertEqual({row["nid"]: row["suggestion"] for row in rows}, database)

    def test_symbol_files_and_stdin_compute_nids_without_a_database(self):
        with tempfile.TemporaryDirectory() as directory:
            names = Path(directory) / "symbols.txt"
            symbols = ["sceExample", "-leading-dash", "  literal spaces  ", "unicode_Ω", "sceExample"]
            names.write_bytes(("\ufeff" + "\r\n".join(symbols) + "\r\n\r\n").encode("utf-8"))
            for source in (str(names), "-"):
                with self.subTest(source=source), patch("sys.stdin", io.StringIO(names.read_text(encoding="utf-8-sig"))), \
                        patch.object(nid_names, "load_db", side_effect=AssertionError("database used")), \
                        contextlib.redirect_stdout(io.StringIO()) as output:
                    self.assertEqual(nid_names.main(["--name", "first", "--from-file", source, "--json"]), 0)
                rows = json.loads(output.getvalue())["symbols"]
                self.assertEqual(rows, [{"name": name, "nid": nid_names.compute_nid(name)} for name in ["first", *symbols]])
            with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit) as exited:
                nid_names.main(["--from-file", str(names), "--nid"])
            self.assertEqual(exited.exception.code, 2)
            with patch.object(nid_names, "load_db", side_effect=AssertionError("database used")), \
                    contextlib.redirect_stdout(io.StringIO()) as output, \
                    contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit) as exited:
                nid_names.main(["--from-file", str(names.parent / "missing"), "--json"])
            self.assertEqual(exited.exception.code, 2)
            self.assertEqual(output.getvalue(), "")



class SourceCollectionTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.prx = self.root / "core" / "libs" / "prx"
        self.module = self.prx / "libSceExample"
        self.module.mkdir(parents=True)
        self.root_patch = patch.object(nid_names, "ROOT", self.root)
        self.prx_patch = patch.object(nid_names, "PRX", self.prx)
        self.root_patch.start()
        self.prx_patch.start()
        self.addCleanup(self.root_patch.stop)
        self.addCleanup(self.prx_patch.stop)

    def write_source(self, text, relative="Export.cpp"):
        path = self.module / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding="utf-8")
        return path

    def cli(self, *arguments):
        tools = self.root / "tools"
        tools.mkdir(exist_ok=True)
        tool = tools / "nid_names.py"
        tool.write_bytes(Path(nid_names.__file__).read_bytes())
        return subprocess.run([sys.executable, str(tool), *map(str, arguments)],
                              env={**os.environ, "PYTHONIOENCODING": "cp1252"},
                              capture_output=True, timeout=10)

    def test_report_preserves_unicode_paths_with_legacy_stdout(self):
        self.write_source('APS5_EXPORT("AAAAAAAAAAA", exampleUnknown00);\n', "\u8cc7\u6599.cpp")
        database = self.root / "aerolib.csv"
        database.write_bytes(b"")
        result = self.cli("--db", database)
        self.assertEqual(result.returncode, 0, result.stderr.decode("utf-8", errors="replace"))
        self.assertIn("core/libs/prx/libSceExample/\u8cc7\u6599.cpp", result.stdout.decode("utf-8"))

    def test_error_preserves_unicode_paths_with_legacy_stderr(self):
        database = self.root / "\u8cc7\u6599"
        database.mkdir()
        result = self.cli("--db", database)
        self.assertEqual(result.returncode, 2, result.stderr.decode("utf-8", errors="replace"))
        self.assertIn("\u8cc7\u6599", result.stderr.decode("utf-8"))
        self.assertNotIn(b"Traceback", result.stderr)

    def test_comments_and_literals_do_not_create_export_records(self):
        self.write_source('''// APS5_EXPORT("AAAAAAAAAAA", exampleUnknown00);
/* int APS5_VABI sceCommented() { return 0; } */
auto example = R"text(APS5_EXPORT("BBBBBBBBBBB", exampleUnknown01);
int APS5_VABI sceQuoted() { return 0; })text";
APS5_EXPORT("CCCCCCCCCCC", exampleUnknown02);
int APS5_VABI sceReal() { return 0; }
''')
        self.assertEqual(nid_names.collect_unknowns(), {"CCCCCCCCCCC": "core/libs/prx/libSceExample/Export.cpp:exampleUnknown02"})
        self.assertEqual(nid_names.collect_real_names(), {"sceReal"})

    def test_assembly_export_macros_are_real_names(self):
        self.write_source('''asm(".text\n"
    APS5_ASM_FUNCTION("sceAsmCall_nid_postfix")
    "ret\n"
    APS5_SIGNAL_ASM_FUNCTION("sigAsmCall_nid_postfix")
    "ret\n");
// APS5_ASM_FUNCTION("sceCommented_nid_postfix")
auto example = "APS5_ASM_FUNCTION(\"sceQuoted_nid_postfix\")";
''')
        self.assertEqual(nid_names.collect_real_names(), {
            "sceAsmCall", "sceAsmCall_nid_postfix",
            "sigAsmCall", "sigAsmCall_nid_postfix",
        })

    def test_disambiguated_definitions_are_real_names_of_their_symbol(self):
        self.write_source('int APS5_VABI fstat_nid_disambig1_nid_postfix(int fd) { return 0; }\n'
                          'int APS5_VABI pwrite_nid_disambig12(int fd) { return 0; }\n'
                          'int APS5_VABI sceKeep_nid_disambigA() { return 0; }\n')
        self.assertEqual(nid_names.collect_real_names(), {
            "fstat", "fstat_nid_disambig1_nid_postfix",
            "pwrite", "pwrite_nid_disambig12",
            "sceKeep_nid_disambigA",
        })

    def test_formatted_alias_macro_is_detected(self):
        self.write_source('APS5_EXPORT (\n "AAAAAAAAAAA" ,\n exampleUnknown00\n );\n')
        self.assertEqual(nid_names.collect_unknowns(), {"AAAAAAAAAAA": "core/libs/prx/libSceExample/Export.cpp:exampleUnknown00"})

    def test_known_aliases_are_not_unknown_exports(self):
        self.write_source('APS5_EXPORT("AAAAAAAAAAA", sceKnownCall);\n'
                          'APS5_EXPORT("BBBBBBBBBBB", exampleUnknown00);\n')
        self.assertEqual(nid_names.collect_unknowns(), {"BBBBBBBBBBB": "core/libs/prx/libSceExample/Export.cpp:exampleUnknown00"})

    def test_windows_source_paths_use_portable_separators(self):
        self.write_source('APS5_EXPORT("AAAAAAAAAAA", exampleUnknown00);\n')
        relative = PureWindowsPath("core/libs/prx/libSceExample/Export.cpp")
        with patch.object(Path, "relative_to", return_value=relative):
            self.assertEqual(nid_names.collect_unknowns(), {"AAAAAAAAAAA": "core/libs/prx/libSceExample/Export.cpp:exampleUnknown00"})

    def test_test_fixtures_are_not_runtime_implementations(self):
        self.write_source('APS5_EXPORT("AAAAAAAAAAA", exampleUnknown00);\nint APS5_VABI sceFixture() { return 0; }\n', "tests/Fixture.cpp")
        self.assertEqual(nid_names.collect_unknowns(), {})
        self.assertEqual(nid_names.collect_real_names(), set())

    def test_unreadable_sources_fail_instead_of_returning_incomplete_names(self):
        self.write_source('int APS5_VABI sceReal() { return 0; }\n')
        with patch.object(Path, "read_text", side_effect=OSError("read denied")):
            with self.assertRaisesRegex(OSError, "read denied"):
                nid_names.collect_unknowns()
            with self.assertRaisesRegex(OSError, "read denied"):
                nid_names.collect_real_names()

    def test_explicit_empty_nid_filter_does_not_scan_the_tree(self):
        cache = self.root / "aerolib.csv"
        cache.write_text("AAAAAAAAAAA example\n", encoding="utf-8")
        output = io.StringIO()
        with patch.object(sys, "argv", ["nid_names.py", "--db", str(cache), "--nid", "--json"]), patch.object(nid_names, "collect_unknowns", side_effect=AssertionError("unexpected tree scan")), contextlib.redirect_stdout(output):
            self.assertEqual(nid_names.main(), 0)
        self.assertEqual(json.loads(output.getvalue())["rows"], [])

    def test_empty_abbreviated_filter_skips_all_source_scans(self):
        cache = self.root / "aerolib.csv"
        cache.write_text("AAAAAAAAAAA example\n", encoding="utf-8")
        arguments = ["nid_names.py", "--db", str(cache), "--ni", "--json"]
        with patch.object(sys, "argv", arguments), patch.object(nid_names, "collect_unknowns", side_effect=AssertionError("unexpected aliases scan")), patch.object(nid_names, "collect_real_names", side_effect=AssertionError("unexpected definitions scan")), contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(nid_names.main(), 0)

    def test_literals_and_digit_separators_keep_following_definitions(self):
        self.write_source('''auto count = 1'000;
auto quote = '\\'';
auto string = "\\\" APS5_VABI sceQuoted() { }";
int APS5_VABI sceReal() { return 0; }
''')
        self.assertEqual(nid_names.collect_real_names(), {"sceReal"})

    def test_name_option_computes_symbol_nid(self):
        output = io.StringIO()
        with patch.object(sys, "argv", ["nid_names.py", "--name", "ceilf", "floorf"]), contextlib.redirect_stdout(output):
            self.assertEqual(nid_names.main(), 0)
        text = output.getvalue()
        self.assertIn("ceilf", text)
        self.assertIn("GAUuLKGhsCw", text)
        self.assertIn("floorf", text)
        self.assertIn("mKhVDmYciWA", text)

    def test_name_option_with_json_emits_symbols_list(self):
        output = io.StringIO()
        with patch.object(sys, "argv", ["nid_names.py", "--name", "ceilf", "--json"]), contextlib.redirect_stdout(output):
            self.assertEqual(nid_names.main(), 0)
        data = json.loads(output.getvalue())
        self.assertEqual(data, {"symbols": [{"name": "ceilf", "nid": "GAUuLKGhsCw"}]})

    def test_symbol_alias_does_not_load_database(self):
        output = io.StringIO()
        with patch.object(sys, "argv", ["nid_names.py", "--symbol", "truncf"]), \
                patch.object(nid_names, "load_db", side_effect=AssertionError("unexpected db load")), \
                contextlib.redirect_stdout(output):
            self.assertEqual(nid_names.main(), 0)
        self.assertIn("Vo8rvWtZw3g", output.getvalue())

    def test_name_option_rejects_nid_and_keeps_long_names(self):
        with patch.object(sys, "argv", ["nid_names.py", "--name", "ceilf", "--nid", "AAAAAAAAAAA"]), \
                contextlib.redirect_stderr(io.StringIO()):
            with self.assertRaises(SystemExit):
                nid_names.main()
        long_name = "a" * 50
        output = io.StringIO()
        with patch.object(sys, "argv", ["nid_names.py", "--name", long_name]), contextlib.redirect_stdout(output):
            self.assertEqual(nid_names.main(), 0)
        self.assertIn(long_name, output.getvalue())

    def test_spliced_line_comment_and_macro_tokens(self):
        self.write_source('// continued \\\nAPS5_EXPORT("AAAAAAAAAAA", exampleUnknown00);\nAPS5_EX\\\nPORT("BBBBBBBBBBB", exampleUnknown01);\n')
        self.assertEqual(nid_names.collect_unknowns(), {"BBBBBBBBBBB": "core/libs/prx/libSceExample/Export.cpp:exampleUnknown01"})

    def test_main_reconfigures_streams(self):
        class Stream:
            def __init__(self):
                self.encoding = None
                self.errors = None

            def reconfigure(self, encoding=None, errors=None):
                self.encoding = encoding
                self.errors = errors

            def write(self, text):
                pass

            def flush(self):
                pass

        out, err = Stream(), Stream()
        with patch.object(nid_names.sys, "stdout", out), patch.object(nid_names.sys, "stderr", err):
            try:
                nid_names.main(["--invalid-argument"])
            except SystemExit:
                pass
        self.assertEqual(out.encoding, "utf-8")
        self.assertEqual(out.errors, "backslashreplace")
        self.assertEqual(err.encoding, "utf-8")
        self.assertEqual(err.errors, "backslashreplace")



if __name__ == "__main__":
    unittest.main()
