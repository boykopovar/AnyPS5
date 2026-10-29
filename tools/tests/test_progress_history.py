import contextlib
import io
import json
import os
import stat
import subprocess
import sys
import tempfile
import unittest
from datetime import date
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import progress
import progress_history

OPCODES = Path("core/shader/recompiler/RdnaDecoder/include/RdnaDecoder/RdnaOpcode.hpp")


def commit(root, day, message, body, time="12:00:00Z"):
    library = root / "core/libs/prx/libSceExample"
    library.mkdir(parents=True, exist_ok=True)
    (library / "Export.cpp").write_text(body, encoding="utf-8")
    (root / OPCODES).parent.mkdir(parents=True, exist_ok=True)
    (root / OPCODES).write_text("enum class RdnaOpcode : int {\n    SEndpgm,\n};\n", encoding="utf-8")
    env = {**os.environ, "GIT_AUTHOR_DATE": f"{day}T{time}", "GIT_COMMITTER_DATE": f"{day}T{time}"}
    subprocess.run(["git", "-C", str(root), "add", "-A"], check=True)
    subprocess.run(["git", "-C", str(root), "-c", "user.name=t", "-c", "user.email=t@t", "commit", "-q", "-m", message],
                   check=True, env=env)


class HistoryReportOutputTests(unittest.TestCase):
    def test_report_output_errors_are_usage_errors_and_allow_retry(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            subprocess.run(["git", "-C", directory, "init", "-q"], check=True)
            commit(root, "2026-01-01", "fixture", "int APS5_VABI A() { return 0; }\n")
            with patch.multiple(progress, ROOT=root, PRX=root / "core/libs/prx", OPCODES=root / OPCODES):
                for index, name in enumerate((None, "history.json", "history.svg", "history.html")):
                    with self.subTest(name=name):
                        output = root / f"reports-{index}"
                        if name is None:
                            output.write_bytes(b"preserve original")
                            blocked = output
                        else:
                            output.mkdir()
                            blocked = output / name
                            blocked.mkdir()
                        stdout, stderr = io.StringIO(), io.StringIO()
                        with contextlib.redirect_stdout(stdout), contextlib.redirect_stderr(stderr), self.assertRaises(SystemExit) as raised:
                            progress_history.main([str(output), "--cache", str(root / "counts.json")])
                        self.assertEqual(raised.exception.code, 2)
                        self.assertIn(str(blocked), stderr.getvalue())
                        self.assertEqual(stdout.getvalue(), "")
                        self.assertNotIn("Traceback", stderr.getvalue())
                        if name is None:
                            self.assertEqual(blocked.read_bytes(), b"preserve original")
                            blocked.unlink()
                        else:
                            blocked.rmdir()
                        with contextlib.redirect_stdout(stdout):
                            progress_history.main([str(output), "--cache", str(root / "counts.json")])
                        data = json.loads((output / "history.json").read_text(encoding="utf-8"))
                        self.assertEqual(data[0]["libraries"]["done"], 1)
                        for filename in ("history.json", "history.svg", "history.html"):
                            self.assertGreater((output / filename).stat().st_size, 0)


class ProgressHistoryTests(unittest.TestCase):
    @unittest.skipUnless(os.name == "nt", "requires Windows read-only replacement semantics")
    def test_readonly_cache_failure_cleans_temporary_and_can_retry(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            cache = root / "history.json"
            subprocess.run(["git", "-C", directory, "init", "-q"], check=True)
            commit(root, "2026-01-01", "first", "int APS5_VABI A() { return 0; }\n")
            with patch.multiple(progress, ROOT=root, PRX=root / "core/libs/prx", OPCODES=root / OPCODES):
                first = progress_history.collect("HEAD", cache)
                previous = cache.read_bytes()
                cache.chmod(stat.S_IREAD)
                commit(root, "2026-01-02", "second", "int APS5_VABI B() { return 0; }\n")
                try:
                    with self.assertRaises(PermissionError):
                        progress_history.collect("HEAD", cache)
                    self.assertEqual(cache.read_bytes(), previous)
                    self.assertEqual(list(root.glob(".history-*")), [])
                finally:
                    cache.chmod(stat.S_IREAD | stat.S_IWRITE)
                    for path in root.glob(".history-*"):
                        path.chmod(stat.S_IREAD | stat.S_IWRITE)
                retried = progress_history.collect("HEAD", cache)
                self.assertEqual(len(retried), 2)
                self.assertEqual(retried[0], first[0])
                self.assertEqual(len(json.loads(cache.read_text(encoding="utf-8"))["commits"]), 2)
                self.assertEqual(list(root.glob(".history-*")), [])

    @unittest.skipIf(os.name == "nt", "POSIX permission bits are not supported on Windows")
    def test_cache_replacement_retains_posix_permissions(self):
        with tempfile.TemporaryDirectory() as directory:
            cache = Path(directory) / "history.json"
            cache.write_bytes(b"previous cache")
            cache.chmod(0o640)
            progress_history.save_cache(cache, "rules", {})
            self.assertEqual(stat.S_IMODE(cache.stat().st_mode), 0o640)
            self.assertEqual(json.loads(cache.read_text(encoding="utf-8"))["rules"], "rules")
            self.assertEqual(list(cache.parent.iterdir()), [cache])

    def test_cache_reuses_immutable_commits_and_invalidates_changed_rules(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            subprocess.run(["git", "-C", directory, "init", "-q"], check=True)
            commit(root, "2026-01-01", "first", "int APS5_VABI A() { return 0; }\n")
            cache = root / "cache" / "history.json"
            isa = root / "isa.txt"
            isa.write_bytes(progress.ISA.read_bytes())
            with patch.multiple(progress, ROOT=root, PRX=root / "core/libs/prx", OPCODES=root / OPCODES, ISA=isa):
                with patch.object(progress_history, "measure", wraps=progress_history.measure) as measure:
                    first = progress_history.collect("HEAD", cache)
                    self.assertEqual(measure.call_count, 1)
                    before = cache.read_bytes()
                    measure.reset_mock()
                    self.assertEqual(progress_history.collect("HEAD", cache), first)
                    measure.assert_not_called()
                    self.assertEqual(cache.read_bytes(), before)
                    commit(root, "2026-01-02", "second", "int APS5_VABI A() { return 0; }\nint APS5_VABI B() { return 0; }\n")
                    measure.reset_mock()
                    second = progress_history.collect("HEAD", cache)
                    self.assertEqual(measure.call_count, 1)
                    self.assertEqual(second[0], first[0])
                    isa.write_bytes(isa.read_bytes() + b"\n# changed rules input\n")
                    measure.reset_mock()
                    self.assertEqual(progress_history.collect("HEAD", cache), second)
                    self.assertEqual(measure.call_count, 2)
                uncached = progress_history.collect("HEAD")
                self.assertEqual(uncached, second)
                self.assertEqual(progress_history.page(uncached), progress_history.page(second))
            self.assertFalse(any(path.name.startswith(".history-") for path in cache.parent.iterdir()))

    def test_invalid_cache_is_not_overwritten(self):
        import json
        with tempfile.TemporaryDirectory() as directory:
            cache = Path(directory) / "history.json"
            for contents in (b"{", b"[]", json.dumps({"schema_version": 1, "rules": progress_history.cache_signature(),
                                                   "commits": {"a" * 40: {"libraries": {"done": True, "total": 1, "percent": 100},
                                                                               "shaders": {"done": 0, "total": 0, "percent": 0}}}}).encode()):
                with self.subTest(contents=contents):
                    cache.write_bytes(contents)
                    with patch.object(progress_history, "measure", side_effect=AssertionError("measurement started")), \
                            self.assertRaises(ValueError):
                        progress_history.collect("HEAD", cache)
                    self.assertEqual(cache.read_bytes(), contents)

    def test_failed_cache_publication_preserves_the_old_file(self):
        with tempfile.TemporaryDirectory() as directory:
            cache = Path(directory) / "history.json"
            cache.write_bytes(b"previous cache")
            with patch.object(progress_history.os, "replace", side_effect=OSError("cannot publish")), \
                    self.assertRaisesRegex(OSError, "cannot publish"):
                progress_history.save_cache(cache, "rules", {})
            self.assertEqual(cache.read_bytes(), b"previous cache")
            self.assertEqual(list(cache.parent.iterdir()), [cache])

    def test_one_point_per_day_at_the_last_commit(self):
        stub = "static void Stub() { NotImplemented_nid_no_patch(__func__); }\n"
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            subprocess.run(["git", "-C", directory, "init", "-q"], check=True)
            commit(root, "2026-01-01", "first", "int APS5_VABI A() { Stub(); return 0; }\n" + stub)
            commit(root, "2026-01-02", "second", "int APS5_VABI A() { return 0; }\nint APS5_VABI B() { return 0; }\n")
            commit(root, "2026-01-02", "third", "int APS5_VABI A() { return 0; }\nint APS5_VABI B() { Stub(); return 0; }\n" + stub)
            with patch.multiple(progress, ROOT=root, PRX=root / "core/libs/prx", OPCODES=root / OPCODES):
                points = progress_history.collect("HEAD")
                html = progress_history.page(points)
        self.assertEqual([p["date"] for p in points], ["2026-01-01", "2026-01-02"])
        self.assertEqual([(p["libraries"]["done"], p["libraries"]["total"]) for p in points], [(0, 1), (1, 2)])
        self.assertIn("<svg", html)
        self.assertIn("2026-01-02", html)

    def test_unknown_ref_is_reported_with_git_reason(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            subprocess.run(["git", "-C", directory, "init", "-q"], check=True)
            commit(root, "2026-01-01", "first", "int APS5_VABI A() { return 0; }\n")
            with patch.multiple(progress, ROOT=root, PRX=root / "core/libs/prx", OPCODES=root / OPCODES), \
                    patch.dict(os.environ, {"LC_ALL": "C"}):
                with self.assertRaises(ValueError) as raised:
                    progress_history.daily_commits("no-such-ref")
        self.assertIn("no-such-ref", str(raised.exception))
        self.assertIn("unknown revision", str(raised.exception))

    def test_days_are_utc_whatever_the_committer_offset(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            subprocess.run(["git", "-C", directory, "init", "-q"], check=True)
            commit(root, "2026-01-02", "older", "int APS5_VABI A() { return 0; }\n", "08:00:00+13:00")
            commit(root, "2026-01-01", "newer", "int APS5_VABI A() { return 0; }\nint APS5_VABI B() { return 0; }\n",
                   "21:30:00+02:00")
            with patch.multiple(progress, ROOT=root, PRX=root / "core/libs/prx", OPCODES=root / OPCODES):
                points = progress_history.collect("HEAD")
        self.assertEqual([(p["date"], p["libraries"]["total"]) for p in points], [("2026-01-01", 2)])

    def test_long_series_keeps_the_chart_readable(self):
        def series(count):
            start = date(2026, 1, 1).toordinal()
            return [{"date": date.fromordinal(start + i).isoformat(),
                     **{name: {"done": i, "total": count, "percent": 100 * i / count} for name in ("libraries", "shaders")}}
                    for i in range(count)]
        short, medium, long = (progress_history.chart(series(count)) for count in (20, 100, 400))
        week = progress_history.chart(series(10))
        self.assertEqual((week.count('class="x"'), week.count("<circle")), (10, 20))
        self.assertEqual((short.count('class="x"'), short.count("<circle")), (3, 40))
        self.assertEqual((medium.count('class="x"'), medium.count("<circle")), (14, 0))
        self.assertEqual((long.count('class="x"'), long.count("<circle")), (14, 0))

    def test_each_panel_shows_its_declared_total(self):
        point = {"date": "2026-01-01", "libraries": {"done": 3292, "total": 3706, "percent": 88.83},
                 "shaders": {"done": 1166, "total": 1166, "percent": 100.0}}
        svg = progress_history.chart([point, {**point, "date": "2026-01-02"}])
        self.assertIn("Declared 3,706", svg)
        self.assertIn("Declared 1,166", svg)
        self.assertIn("Implemented 1,166 (100%)", svg)

    def test_axis_labels_fall_on_calendar_days_without_commits(self):
        start = date(2026, 8, 3).toordinal()
        skipped = {date(2026, 8, 10), date(2026, 8, 17), date(2026, 8, 31)}
        points = [{"date": day.isoformat(),
                   **{name: {"done": i, "total": 70, "percent": 100 * i / 70} for name in ("libraries", "shaders")}}
                  for i, day in enumerate(date.fromordinal(start + i) for i in range(70)) if day not in skipped]
        svg = progress_history.chart(points)
        for label in ("Aug 3", "Aug 10", "Aug 17", "Aug 24", "Aug 31"):
            self.assertIn(f">{label}<", svg)

    def test_main_writes_utf8_output_files(self):
        point = {"date": "2026-01-01", "commit": "abc1234",
                 "libraries": {"done": 1, "total": 2, "percent": 50.0},
                 "shaders": {"done": 3, "total": 4, "percent": 75.0}}
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "site"
            with patch.object(progress_history, "collect", return_value=[point]):
                progress_history.main([str(output)])
            svg = (output / "history.svg").read_text(encoding="utf-8")
            self.assertIn("\u25cf", svg)
            html = (output / "history.html").read_text(encoding="utf-8")
            self.assertIn("<!DOCTYPE html>", html)
            data = json.loads((output / "history.json").read_text(encoding="utf-8"))
            self.assertEqual(data, [point])

    def test_main_rejects_aliased_output_reports(self):
        point = {"date": "2026-01-01", "commit": "abc1234",
                 "libraries": {"done": 1, "total": 2, "percent": 50.0},
                 "shaders": {"done": 3, "total": 4, "percent": 75.0}}
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "site"
            output.mkdir(parents=True)
            report = output / "history.json"
            alias = output / "history.html"
            report.write_text("existing json", encoding="utf-8")
            try:
                os.link(report, alias)
            except OSError:
                self.skipTest("hard links unavailable")
            errors = io.StringIO()
            with patch.object(progress_history, "collect", return_value=[point]):
                with contextlib.redirect_stderr(errors), self.assertRaises(SystemExit) as error:
                    progress_history.main([str(output)])
            self.assertEqual(error.exception.code, 2)
            self.assertEqual(report.read_text(encoding="utf-8"), "existing json")
            self.assertEqual(alias.read_text(encoding="utf-8"), "existing json")

    def test_main_symbolic_link_loops_do_not_escape_as_tracebacks(self):
        point = {"date": "2026-01-01", "commit": "abc1234",
                 "libraries": {"done": 1, "total": 2, "percent": 50.0},
                 "shaders": {"done": 3, "total": 4, "percent": 75.0}}
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            loop = root / "loop"
            try:
                os.symlink(loop, loop)
            except OSError:
                self.skipTest("symbolic links unavailable")
            with self.subTest("cache"):
                output = root / "site"
                with patch.object(progress_history, "collect", return_value=[point]) as collect:
                    with contextlib.redirect_stdout(io.StringIO()):
                        progress_history.main([str(output), "--cache", str(loop)])
                collect.assert_called_once_with("HEAD", loop)
                self.assertEqual(json.loads((output / "history.json").read_text(encoding="utf-8")), [point])
            with self.subTest("output"):
                errors = io.StringIO()
                with patch.object(progress_history, "collect", return_value=[point]):
                    with contextlib.redirect_stderr(errors), self.assertRaises(SystemExit) as error:
                        progress_history.main([str(loop), "--cache", str(root / "counts.json")])
                self.assertEqual(error.exception.code, 2)
                self.assertIn(str(loop), errors.getvalue())


    def test_cache_recursion_is_reported_as_a_path_specific_value_error(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "counts.json"
            path.write_bytes(b"preserve cache")
            with patch.object(progress_history.json, "loads", side_effect=RecursionError("decoder depth")):
                with self.assertRaisesRegex(ValueError, "history cache JSON is too deeply nested") as raised:
                    progress_history.load_cache(path, "signature")
            self.assertIn(str(path), str(raised.exception))
            self.assertEqual(path.read_bytes(), b"preserve cache")

    def test_real_deep_cache_cli_is_bounded_and_preserves_files(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            cache = root / "counts.json"
            original = ("[" * 20000 + "0" + "]" * 20000).encode()
            cache.write_bytes(original)
            completed = subprocess.run([sys.executable, str(Path(progress_history.__file__)), str(root / "reports"), "--cache", str(cache)], capture_output=True, timeout=10)
            self.assertEqual(completed.returncode, 2, completed.stderr)
            self.assertEqual(completed.stdout, b"")
            self.assertNotIn(b"Traceback", completed.stderr)
            self.assertIn(b"counts.json", completed.stderr)
            self.assertFalse((root / "reports").exists())
            self.assertEqual(cache.read_bytes(), original)




if __name__ == "__main__":
    unittest.main()
