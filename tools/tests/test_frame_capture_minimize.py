from pathlib import Path
from types import SimpleNamespace
import contextlib
import io
import json
import re
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import frame_capture_minimize


class PredicateMinimizer(frame_capture_minimize.Minimizer):
    def __init__(self, required_calls, required_chunks):
        super().__init__(SimpleNamespace(max_trials=1000, filter="filter", replay="replay", capture="capture", output="output"), re.compile("failure"))
        self.required_calls = set(required_calls)
        self.required_chunks = set(required_chunks)

    def predicate(self, calls, chunks):
        self.trials += 1
        return self.required_calls.issubset(set(range(20)) - calls) and self.required_chunks.issubset(set(range(20)) - chunks)


class FrameCaptureMinimizeTests(unittest.TestCase):
    def empty_cli(self, root, filter_source=None, replay_source=None, output=None):
        capture = root / "capture"
        capture.mkdir()
        capture_filter = root / "filter.py"
        capture_filter.write_text(filter_source or """import json, pathlib, sys
command, source, *rest = sys.argv[1:]
if command == 'inspect':
    print(json.dumps({'calls': [], 'chunks': []}))
else:
    target = pathlib.Path(rest[1])
    if target.exists() or target.is_symlink():
        print('destination already exists', file=sys.stderr)
        raise SystemExit(2)
    target.mkdir(parents=True)
""", encoding="utf-8")
        replay = root / "replay.py"
        replay.write_text(replay_source or "print('failure')\nraise SystemExit(7)\n", encoding="utf-8")
        output = output or root / "result"
        arguments = ["--capture", str(capture), "--replay", str(replay), "--filter", str(capture_filter), "--output", str(output), "--expect-exit-code", "7", "--match-regex", "failure"]
        return arguments, capture_filter, replay, output

    def test_minimizes_calls_and_chunks_while_preserving_required_units(self):
        minimizer = PredicateMinimizer({2}, {4})
        removed_calls = set()
        removed_chunks = set()
        calls, removed_calls, removed_chunks, calls_complete = minimizer.minimize([0, 2, 3], removed_calls, removed_chunks, True)
        chunks, removed_calls, removed_chunks, chunks_complete = minimizer.minimize([1, 4, 5], removed_calls, removed_chunks, False)
        self.assertTrue(calls_complete)
        self.assertTrue(chunks_complete)
        self.assertEqual(calls, [2])
        self.assertEqual(chunks, [4])
        self.assertEqual(removed_calls, {0, 3})
        self.assertEqual(removed_chunks, {1, 5})

    def test_requires_repeated_confirmation_and_positive_trial_limit(self):
        for arguments in (("--confirm-runs", "1"), ("--max-trials", "0"), ("--replay-timeout", "0")):
            with self.subTest(arguments=arguments), self.assertRaises(SystemExit):
                frame_capture_minimize.parse_args(["--capture", "in", "--replay", "run", "--filter", "filter", "--output", "out", "--match-regex", "failure", "--expect-exit-code", "1", *arguments])

    def test_cli_keeps_only_units_needed_for_a_repeated_failure(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            capture = root / "capture"
            capture.mkdir()
            (capture / "capture.json").write_text(json.dumps({
                "calls": [
                    {"index": 0, "kind": "frame-start", "memory_group": None},
                    {"index": 1, "kind": "submit", "memory_group": None},
                    {"index": 2, "kind": "end", "memory_group": None},
                ],
                "chunks": [
                    {"index": 0, "kind": "range", "group": 0},
                    {"index": 1, "kind": "snapshot", "group": 0, "name": "trigger"},
                    {"index": 2, "kind": "snapshot", "group": 0, "name": "noise"},
                ],
            }), encoding="utf-8")
            capture_filter = root / "filter.py"
            capture_filter.write_text("""import json, pathlib, sys
command, source, *rest = sys.argv[1:]
data = json.loads((pathlib.Path(source) / 'capture.json').read_text())
if command == 'inspect':
    print(json.dumps(data))
else:
    plan_path, destination = rest
    lines = pathlib.Path(plan_path).read_text().splitlines()[1:]
    calls = {int(line.split()[1]) for line in lines if line.startswith('call ')}
    chunks = {int(line.split()[1]) for line in lines if line.startswith('chunk ')}
    data['calls'] = [item for item in data['calls'] if item['index'] not in calls]
    data['chunks'] = [item for item in data['chunks'] if item['kind'] == 'range' or item['index'] not in chunks]
    target = pathlib.Path(destination)
    target.mkdir()
    (target / 'capture.json').write_text(json.dumps(data))
""", encoding="utf-8")
            replay = root / "replay.py"
            replay.write_text("""import json, pathlib, sys
data = json.loads((pathlib.Path(sys.argv[1]) / 'capture.json').read_text())
failed = any(call['kind'] == 'submit' for call in data['calls']) and any(chunk.get('name') == 'trigger' for chunk in data['chunks'])
print('frame failure observed' if failed else 'no failure')
raise SystemExit(7 if failed else 0)
""", encoding="utf-8")
            output = root / "minimized"
            arguments = ["--capture", str(capture), "--replay", str(replay), "--filter", str(capture_filter), "--output", str(output), "--expect-exit-code", "7", "--match-regex", "frame failure observed", "--confirm-runs", "2", "--max-trials", "20"]
            stdout = io.StringIO()
            stderr = io.StringIO()
            with contextlib.redirect_stdout(stdout), contextlib.redirect_stderr(stderr):
                result = frame_capture_minimize.main(arguments)
            self.assertEqual(result, 0, stderr.getvalue())
            minimized = json.loads((output / "capture.json").read_text(encoding="utf-8"))
            self.assertEqual([call["kind"] for call in minimized["calls"]], ["frame-start", "submit", "end"])
            self.assertEqual([chunk.get("name", chunk["kind"]) for chunk in minimized["chunks"]], ["range", "trigger"])
            self.assertIn("final predicate confirmed 2 times", stdout.getvalue())

    def test_cli_reports_a_non_ascii_output_path_as_utf8(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            arguments, _, _, output = self.empty_cli(root, output=root / "界" / "result")
            stdout = io.TextIOWrapper(io.BytesIO(), encoding="cp1252")
            stderr = io.StringIO()
            with contextlib.redirect_stdout(stdout), contextlib.redirect_stderr(stderr):
                result = frame_capture_minimize.main(arguments)
                stdout.flush()
            self.assertEqual(result, 0, stderr.getvalue())
            self.assertIn(f"wrote {output};", stdout.buffer.getvalue().decode("utf-8"))

    def test_cli_refuses_to_reduce_when_the_baseline_does_not_match(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            capture = root / "capture"
            capture.mkdir()
            (capture / "capture.json").write_text(json.dumps({"calls": [], "chunks": []}), encoding="utf-8")
            capture_filter = root / "filter.py"
            capture_filter.write_text("""import json, pathlib, sys
command, source, *rest = sys.argv[1:]
data = json.loads((pathlib.Path(source) / 'capture.json').read_text())
if command == 'inspect':
    print(json.dumps(data))
""", encoding="utf-8")
            replay = root / "replay.py"
            replay.write_text("print('normal replay')\n", encoding="utf-8")
            output = root / "must-not-exist"
            arguments = ["--capture", str(capture), "--replay", str(replay), "--filter", str(capture_filter), "--output", str(output), "--expect-exit-code", "0", "--match-regex", "failure", "--confirm-runs", "2"]
            stdout = io.StringIO()
            stderr = io.StringIO()
            with contextlib.redirect_stdout(stdout), contextlib.redirect_stderr(stderr):
                result = frame_capture_minimize.main(arguments)
            self.assertEqual(result, 1)
            self.assertFalse(output.exists())
            self.assertIn("baseline did not reproduce", stderr.getvalue())


    def test_output_exists_is_not_overwritten(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            output = root / "result"
            output.write_text("keep", encoding="utf-8")
            arguments, _, _, _ = self.empty_cli(root, output=output)
            with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
                result = frame_capture_minimize.main(arguments)
            self.assertEqual(result, 2)
            self.assertEqual(output.read_text(encoding="utf-8"), "keep")

    def test_invalid_match_regex_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            arguments, _, _, output = self.empty_cli(root)
            arguments[arguments.index("failure")] = "["
            stderr = io.StringIO()
            with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(stderr):
                result = frame_capture_minimize.main(arguments)
            self.assertEqual(result, 2)
            self.assertIn("invalid --match-regex", stderr.getvalue())
            self.assertFalse(output.exists())

    def test_inspection_rejects_invalid_json(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            bad_filter = "import sys\nprint('not json')\n"
            arguments, _, _, output = self.empty_cli(root, filter_source=bad_filter)
            stderr = io.StringIO()
            with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(stderr):
                result = frame_capture_minimize.main(arguments)
            self.assertEqual(result, 2)
            self.assertIn("invalid inspection JSON", stderr.getvalue())
            self.assertFalse(output.exists())

    def test_inspection_requires_json_object(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            arguments, _, _, output = self.empty_cli(root, filter_source="print('[]')\n")
            stderr = io.StringIO()
            with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(stderr):
                result = frame_capture_minimize.main(arguments)
            self.assertEqual(result, 2)
            self.assertIn("must be a JSON object", stderr.getvalue())
            self.assertFalse(output.exists())

    def test_inspection_entries_require_objects_with_indexes_and_kinds(self):
        invalid_entries = ([None], [{"index": 0}], [{"index": -1, "kind": "submit"}], [{"index": True, "kind": "submit"}], [{"index": 0, "kind": 3}], [{"index": 2, "kind": "submit"}], [{"index": 0, "kind": "unexpected"}])
        for entries in invalid_entries:
            with self.subTest(entries=entries), self.assertRaises(RuntimeError):
                frame_capture_minimize.Minimizer.validate_entries(entries, "calls")

    def test_inspection_entries_accept_nonnegative_integer_indexes(self):
        self.assertIsNone(frame_capture_minimize.Minimizer.validate_entries([{"index": 0, "kind": "submit"}], "calls"))

    def test_inspection_entries_reject_duplicate_indexes(self):
        with self.assertRaisesRegex(RuntimeError, "duplicate index 0"):
            frame_capture_minimize.Minimizer.validate_entries([{"index": 0, "kind": "submit"}, {"index": 0, "kind": "flip"}], "calls")

    def test_inspection_memory_entries_require_valid_groups(self):
        for entry in ({"index": 0, "kind": "memory"}, {"index": 0, "kind": "memory", "memory_group": -1}, {"index": 0, "kind": "memory", "memory_group": True}):
            with self.subTest(entry=entry), self.assertRaisesRegex(RuntimeError, "invalid memory group"):
                frame_capture_minimize.Minimizer.validate_entries([entry], "calls")

    def test_inspection_chunks_require_valid_groups(self):
        for entry in ({"index": 0, "kind": "delta"}, {"index": 0, "kind": "range", "group": -1}, {"index": 0, "kind": "snapshot", "group": True}):
            with self.subTest(entry=entry), self.assertRaisesRegex(RuntimeError, "invalid group"):
                frame_capture_minimize.Minimizer.validate_entries([entry], "chunks")

    def test_inspection_requires_calls_list(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            arguments, _, _, _ = self.empty_cli(root, filter_source="print('{\"chunks\": []}')\n")
            stderr = io.StringIO()
            with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(stderr):
                result = frame_capture_minimize.main(arguments)
            self.assertEqual(result, 2)
            self.assertIn("missing calls or chunks", stderr.getvalue())

    def test_inspection_requires_chunks_list(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            arguments, _, _, _ = self.empty_cli(root, filter_source="print('{\"calls\": []}')\n")
            stderr = io.StringIO()
            with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(stderr):
                result = frame_capture_minimize.main(arguments)
            self.assertEqual(result, 2)
            self.assertIn("missing calls or chunks", stderr.getvalue())

    def test_inspection_reports_filter_stderr(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            script = "import sys\nprint('inspect failed', file=sys.stderr)\nraise SystemExit(8)\n"
            arguments, _, _, _ = self.empty_cli(root, filter_source=script)
            stderr = io.StringIO()
            with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(stderr):
                result = frame_capture_minimize.main(arguments)
            self.assertEqual(result, 2)
            self.assertIn("inspect failed", stderr.getvalue())

    def test_baseline_requires_expected_exit_code(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            arguments, _, _, output = self.empty_cli(root, replay_source="print('failure')\nraise SystemExit(0)\n")
            stderr = io.StringIO()
            with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(stderr):
                result = frame_capture_minimize.main(arguments)
            self.assertEqual(result, 1)
            self.assertIn("baseline did not reproduce", stderr.getvalue())
            self.assertFalse(output.exists())

    def test_baseline_matches_stderr(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            arguments, _, _, output = self.empty_cli(root, replay_source="import sys\nprint('failure', file=sys.stderr)\nraise SystemExit(7)\n")
            with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
                result = frame_capture_minimize.main(arguments)
            self.assertEqual(result, 0)
            self.assertTrue(output.is_dir())

    def test_replay_timeout_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            replay = root / "slow.py"
            replay.write_text("import time\ntime.sleep(1)\n", encoding="utf-8")
            workdir = root / "run"
            workdir.mkdir()
            args = SimpleNamespace(filter="filter", replay=replay, capture=root / "capture", output=root / "output", replay_arg=[], replay_timeout=0.01, expect_exit_code=0)
            minimizer = frame_capture_minimize.Minimizer(args, re.compile("failure"))
            matched, detail = minimizer.replay_once(root / "capture", workdir)
            self.assertFalse(matched)
            self.assertIn("timed out", detail)

    def test_write_plan_sorts_indexes(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "plan"
            frame_capture_minimize.Minimizer.write_plan(path, {8, 2}, {9, 1})
            self.assertEqual(path.read_text(encoding="ascii"), "aps5-frame-filter 1\ncall 2\ncall 8\nchunk 1\nchunk 9\n")

    def test_chunk_reduction_keeps_deltas_for_retained_memory_calls(self):
        description = {
            "calls": [{"index": 0, "memory_group": 4}, {"index": 1, "memory_group": 4}],
            "chunks": [{"index": 0, "kind": "range", "group": 4}, {"index": 1, "kind": "delta", "group": 4}, {"index": 2, "kind": "snapshot", "group": 4}],
        }
        units, removed_groups = frame_capture_minimize.Minimizer.reducible_chunks(description, {0})
        self.assertEqual(units, [1, 2])
        self.assertEqual(removed_groups, set())

    def test_chunk_reduction_auto_removes_deltas_for_deleted_groups(self):
        description = {
            "calls": [{"index": 0, "memory_group": 4}],
            "chunks": [{"index": 0, "kind": "range", "group": 4}, {"index": 1, "kind": "delta", "group": 4}, {"index": 2, "kind": "snapshot", "group": 4}],
        }
        units, removed_groups = frame_capture_minimize.Minimizer.reducible_chunks(description, {0})
        self.assertEqual(units, [2])
        self.assertEqual(removed_groups, {4})

    def test_python_filter_command_uses_current_interpreter(self):
        self.assertEqual(frame_capture_minimize.Minimizer.command(Path("filter.py")), [sys.executable, "filter.py"])

    def test_nonexistent_temp_dir_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            arguments, _, _, _ = self.empty_cli(root)
            arguments.extend(["--temp-dir", str(root / "missing")])
            with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
                frame_capture_minimize.parse_args(arguments)

    def test_predicate_enforces_trial_budget(self):
        args = SimpleNamespace(max_trials=0, filter="filter", replay="replay", capture="capture", output="output")
        minimizer = frame_capture_minimize.Minimizer(args, re.compile("failure"))
        with self.assertRaises(frame_capture_minimize.TrialLimitReached):
            minimizer.predicate(set(), set())

    def test_filter_apply_failure_is_reported(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            script = "import json, sys\nif sys.argv[1] == 'inspect': print('{\"calls\": [], \"chunks\": []}')\nelse: print('filter failed', file=sys.stderr); raise SystemExit(4)\n"
            arguments, _, _, output = self.empty_cli(root, filter_source=script)
            stderr = io.StringIO()
            with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(stderr):
                result = frame_capture_minimize.main(arguments)
            self.assertEqual(result, 2)
            self.assertIn("filter failed", stderr.getvalue())
            self.assertFalse(output.exists())

    def test_cli_creates_missing_output_parent(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            output = root / "new-parent" / "result"
            arguments, _, _, _ = self.empty_cli(root, output=output)
            with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
                result = frame_capture_minimize.main(arguments)
            self.assertEqual(result, 0)
            self.assertTrue(output.is_dir())

    def test_error_does_not_remove_output_created_after_initial_check(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            arguments, _, _, output = self.empty_cli(root)

            def output_appears_during_inspection(minimizer):
                minimizer.output.mkdir()
                (minimizer.output / "keep.txt").write_text("owned elsewhere", encoding="utf-8")
                return {"calls": [], "chunks": []}

            with patch.object(frame_capture_minimize.Minimizer, "inspect", output_appears_during_inspection):
                with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
                    result = frame_capture_minimize.main(arguments)

            self.assertEqual(result, 2)
            self.assertEqual((output / "keep.txt").read_text(encoding="utf-8"), "owned elsewhere")

    def test_baseline_and_final_confirmation_counts(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            counter = root / "runs"
            replay = "import pathlib, sys\np = pathlib.Path(sys.argv[2])\np.write_text(str(int(p.read_text()) + 1) if p.exists() else '1')\nprint('failure')\nraise SystemExit(7)\n"
            arguments, _, _, _ = self.empty_cli(root, replay_source=replay)
            arguments.extend(["--replay-arg", str(counter)])
            with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
                result = frame_capture_minimize.main(arguments)
            self.assertEqual(result, 0)
            self.assertEqual(counter.read_text(encoding="utf-8"), "4")

    def test_failed_final_confirmation_removes_output(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            replay = "import pathlib, sys\nif pathlib.Path(sys.argv[1]).name == 'result': print('normal') ; raise SystemExit(0)\nprint('failure')\nraise SystemExit(7)\n"
            arguments, _, _, output = self.empty_cli(root, replay_source=replay)
            stderr = io.StringIO()
            with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(stderr):
                result = frame_capture_minimize.main(arguments)
            self.assertEqual(result, 1)
            self.assertIn("final capture failed confirmation", stderr.getvalue())
            self.assertFalse(output.exists())

    def test_replay_arg_expands_trial_directory(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            replay = root / "replay.py"
            replay.write_text("import pathlib, sys\npathlib.Path(sys.argv[2], 'received').touch()\nprint('failure')\n", encoding="utf-8")
            workdir = root / "run"
            workdir.mkdir()
            args = SimpleNamespace(filter="filter", replay=replay, capture=root / "capture", output=root / "output", replay_arg=["{trial_dir}"], replay_timeout=2, expect_exit_code=0)
            minimizer = frame_capture_minimize.Minimizer(args, re.compile("failure"))
            matched, _ = minimizer.replay_once(root / "capture", workdir)
            self.assertTrue(matched)
            self.assertTrue((workdir / "received").exists())

    def test_zero_trial_limit_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            arguments, _, _, _ = self.empty_cli(root)
            arguments.extend(["--max-trials", "0"])
            with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
                frame_capture_minimize.parse_args(arguments)

    def test_zero_replay_timeout_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            arguments, _, _, _ = self.empty_cli(root)
            arguments.extend(["--replay-timeout", "0"])
            with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
                frame_capture_minimize.parse_args(arguments)

if __name__ == "__main__":
    unittest.main()
