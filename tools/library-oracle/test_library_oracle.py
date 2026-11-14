import hashlib
import io
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import library_oracle


class Done:
    def __init__(self, returncode, stdout, stderr):
        self.returncode, self.stdout, self.stderr = returncode, stdout.encode(), stderr.encode()


class Script:
    def __init__(self, *outcomes):
        self.outcomes = list(outcomes)
        self.commands = []

    def __call__(self, command, **_):
        self.commands.append(command)
        outcome = self.outcomes.pop(0)
        if isinstance(outcome, BaseException):
            raise outcome
        return outcome


class ReplayTests(unittest.TestCase):
    def test_a_crash_is_recorded_and_the_case_skipped_on_the_next_run(self):
        run = Script(Done(3, "DIFF\ta\tf(i:1) ret console=0x1 anyps5=0x0\n", "RUN\ta\nRUN\tb\n"),
                     Done(0, "DIFF\ta\tf(i:1) ret console=0x1 anyps5=0x0\nSUMMARY\tcompared 1\tmismatched 1\tskipped 1\n", "RUN\ta\n"))
        findings, summary = library_oracle.replay("replay", "modules", "x.golden.tsv", {"s"}, 5, run)
        self.assertEqual(findings, {"a": ("diff", "f(i:1) ret console=0x1 anyps5=0x0"), "b": ("crash", "exit code 3")})
        self.assertEqual(summary, "compared 1 mismatched 1 skipped 1")
        self.assertEqual(run.commands[0][2:4], ["--skip", "s"])
        self.assertEqual(run.commands[1][2:4], ["--skip", "b,s"])

    def test_a_hang_is_recorded_from_the_last_started_case(self):
        run = Script(subprocess.TimeoutExpired("replay", 5, output=b"", stderr=b"RUN\tc\n"),
                     Done(0, "MISSING\td\tsceMissing\nSUMMARY\tcompared 0\tmismatched 0\tskipped 2\n", ""))
        findings, _ = library_oracle.replay("replay", "modules", "x.golden.tsv", set(), 5, run)
        self.assertEqual(findings, {"c": ("hang", "timed out after 5 s"), "d": ("missing", "sceMissing")})

    def test_an_escaping_exception_is_recorded_with_its_message(self):
        run = Script(Done(3, "", "RUN\te\nterminate called after throwing an instance of 'std::runtime_error'\n  what():  bad packet\n"),
                     Done(0, "SUMMARY\tcompared 0\tmismatched 0\tskipped 1\n", ""))
        findings, _ = library_oracle.replay("replay", "modules", "x.golden.tsv", set(), 5, run)
        self.assertEqual(findings, {"e": ("throw", "bad packet")})

    def test_a_caught_exception_is_recorded_as_a_throw(self):
        run = Script(Done(0, "DIFF\tg\tsceAgcCbBranch(dcb:1024) threw 'reserved bits are set'\nSUMMARY\tcompared 1\tmismatched 1\tskipped 0\n", "RUN\tg\n"))
        findings, _ = library_oracle.replay("replay", "modules", "x.golden.tsv", set(), 5, run)
        self.assertEqual(findings, {"g": ("throw", "sceAgcCbBranch(dcb:1024) threw 'reserved bits are set'")})

    def test_a_failure_before_any_case_stops(self):
        run = Script(Done(139, "", "loader error\n"))
        with self.assertRaises(SystemExit):
            library_oracle.replay("replay", "modules", "x.golden.tsv", set(), 5, run)


class KnownTests(unittest.TestCase):
    def test_classify_separates_new_known_and_now_matching_findings(self):
        known = {"a": ("diff", "#1"), "b": ("crash", "#2"), "c": ("diff", "#3"), "e": ("crash", "#4"), "s": ("skip", "env")}
        findings = {"a": ("diff", "x"), "b": ("diff", "y"), "d": ("hang", "z"), "e": ("hang", "w")}
        new, expected, stale = library_oracle.classify(known, findings, {"a", "b", "c", "d", "e"})
        self.assertEqual([case for case, *_ in new], ["b", "d"])
        self.assertEqual([case for case, *_ in expected], ["a", "e"])
        self.assertEqual(stale, [("c", "diff", "#3")])

    def test_write_known_keeps_reasons_and_skips(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "known.tsv"
            path.write_text("# header\na\tdiff\t#1\ns\tskip\tenvironment\nold\tdiff\tgone\n", encoding="utf-8")
            known = library_oracle.load_known(path)
            library_oracle.write_known(path, known, {"a": ("diff", "x"), "n": ("crash", "y")})
            self.assertEqual(path.read_text(encoding="utf-8"), "a\tdiff\t#1\nn\tcrash\ns\tskip\tenvironment\n")

    def test_an_unknown_kind_is_refused(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "known.tsv"
            path.write_text("a\tbroken\treason\n", encoding="utf-8")
            with self.assertRaises(SystemExit):
                library_oracle.load_known(path)

    def test_the_committed_known_file_parses(self):
        known = library_oracle.load_known(library_oracle.HERE / "known.tsv")
        self.assertTrue(known)
        self.assertTrue(all(reason for _, reason in known.values()))


class FetchTests(unittest.TestCase):
    def test_fetch_checks_the_pinned_digest_and_caches(self):
        data = b"# console-oracle golden v1\n"
        calls = []

        def opener(url, timeout):
            calls.append(url)
            return io.BytesIO(data)

        original = dict(library_oracle.FAMILIES)
        try:
            library_oracle.FAMILIES["rtc"] = hashlib.sha256(data).hexdigest()
            with tempfile.TemporaryDirectory() as directory:
                path = library_oracle.fetch("rtc", Path(directory), opener)
                self.assertEqual(path.read_bytes(), data)
                library_oracle.fetch("rtc", Path(directory), opener)
                self.assertEqual(len(calls), 1)
                self.assertIn(library_oracle.PROSPER, calls[0])
                library_oracle.FAMILIES["rtc"] = "0" * 64
                path.unlink()
                with self.assertRaises(SystemExit):
                    library_oracle.fetch("rtc", Path(directory), opener)
        finally:
            library_oracle.FAMILIES.clear()
            library_oracle.FAMILIES.update(original)


class ModuleTests(unittest.TestCase):
    def test_required_modules_asks_the_replay_for_the_chain_it_resolves_through(self):
        run = Script(Done(0, "MODULE\tlibc.prx\nMODULE\tlibkernel.prx\n", ""))
        self.assertEqual(library_oracle.required_modules("replay", run), ["libc.prx", "libkernel.prx"])
        self.assertEqual(run.commands, [["replay", "--required-modules"]])

    def test_required_modules_stays_quiet_about_a_replay_that_does_not_answer(self):
        run = Script(Done(2, "", "usage: replay MODULE_DIRECTORY ...\n"))
        self.assertIsNone(library_oracle.required_modules("replay", run))

    def test_required_modules_stays_quiet_when_the_replay_cannot_be_started(self):
        run = Script(FileNotFoundError(2, "No such file or directory"))
        self.assertIsNone(library_oracle.required_modules("replay", run))

    def test_absent_modules_names_the_chain_files_the_directory_lacks(self):
        with tempfile.TemporaryDirectory() as directory:
            (Path(directory) / "libc.prx").write_bytes(b"")
            (Path(directory) / "libScePosix.prx").mkdir()
            absent = library_oracle.absent_modules(directory, ["libc.prx", "libkernel_sys.prx", "libScePosix.prx"])
            self.assertEqual(absent, ["libkernel_sys.prx", "libScePosix.prx"])

    def test_the_warning_names_the_absent_files_and_the_directory(self):
        note = library_oracle.module_warning(["libkernel_sys.prx"], "/m/unpatched") or ""
        self.assertIn("libkernel_sys.prx is not in /m/unpatched", note)

    def test_there_is_no_warning_without_an_absent_file(self):
        self.assertIsNone(library_oracle.module_warning([], "/m/unpatched"))

    def test_a_run_without_a_missing_case_never_starts_the_replay_for_the_chain(self):
        run = Script(Done(0, "MODULE\tlibkernel_sys.prx\n", ""))
        self.assertIsNone(library_oracle.missing_warning("replay", "/m/unpatched", {"a": ("diff", "ret 0x1")}, run))
        self.assertEqual(run.commands, [])

    def test_a_missing_case_asks_the_replay_and_names_the_files_the_directory_lacks(self):
        run = Script(Done(0, "MODULE\tlibc.prx\nMODULE\tlibkernel_sys.prx\n", ""))
        with tempfile.TemporaryDirectory() as directory:
            (Path(directory) / "libc.prx").write_bytes(b"")
            note = library_oracle.missing_warning("replay", directory, {"a": ("missing", "scePthreadBarrierInit")}, run) or ""
            self.assertEqual(run.commands, [["replay", "--required-modules"]])
            self.assertIn(f"libkernel_sys.prx is not in {directory}", note)


if __name__ == "__main__":
    unittest.main()
