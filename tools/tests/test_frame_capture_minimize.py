from pathlib import Path
from types import SimpleNamespace
import contextlib
import io
import json
import re
import sys
import tempfile
import unittest

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
    pathlib.Path(rest[1]).mkdir(parents=True)
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

if __name__ == "__main__":
    unittest.main()
