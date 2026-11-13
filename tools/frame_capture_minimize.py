import argparse
import json
import math
import os
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path


class TrialLimitReached(Exception):
    pass


class Minimizer:
    CALL_KINDS = {
        "register-shader", "resolve-shader-abi", "resolve-graphics-abi",
        "resolve-graphics-stages-abi", "register-video-output", "unregister-video-output",
        "set-tf-ring", "submit", "flip", "reserve-eop-flip", "suspend", "memory",
        "frame-start", "end", "display-buffer",
    }
    CHUNK_KINDS = {"range", "snapshot", "delta", "reference"}

    def __init__(self, args, pattern):
        self.args = args
        self.pattern = pattern
        self.trials = 0
        self.filter = Path(args.filter).resolve()
        self.replay = Path(args.replay).resolve()
        self.capture = Path(args.capture).resolve()
        self.output = Path(os.path.abspath(args.output))
        self.output_created = False

    @staticmethod
    def command(path):
        return [sys.executable, str(path)] if path.suffix == ".py" else [str(path)]

    @staticmethod
    def validate_entries(entries, label):
        indexes = set()
        for position, entry in enumerate(entries):
            if not isinstance(entry, dict):
                raise RuntimeError(f"capture inspection {label}[{position}] must be an object")
            index = entry.get("index")
            if isinstance(index, bool) or not isinstance(index, int) or index < 0:
                raise RuntimeError(f"capture inspection {label}[{position}] has an invalid index")
            if index in indexes:
                raise RuntimeError(f"capture inspection {label} contains duplicate index {index}")
            indexes.add(index)
            if index != position:
                raise RuntimeError(f"capture inspection {label}[{position}] has a non-sequential index")
            kind = entry.get("kind")
            if not isinstance(kind, str):
                raise RuntimeError(f"capture inspection {label}[{position}] is missing a kind")
            valid_kinds = Minimizer.CALL_KINDS if label == "calls" else Minimizer.CHUNK_KINDS
            if kind not in valid_kinds:
                raise RuntimeError(f"capture inspection {label}[{position}] has an unknown kind {kind!r}")
            if label == "calls" and kind == "memory":
                group = entry.get("memory_group")
                if isinstance(group, bool) or not isinstance(group, int) or group < 0:
                    raise RuntimeError(f"capture inspection calls[{position}] has an invalid memory group")
            if label == "chunks":
                group = entry.get("group")
                if isinstance(group, bool) or not isinstance(group, int) or group < 0:
                    raise RuntimeError(f"capture inspection chunks[{position}] has an invalid group")

    def inspect(self):
        result = subprocess.run([*self.command(self.filter), "inspect", str(self.capture)], capture_output=True, text=True, encoding="utf-8", errors="replace")
        if result.returncode != 0:
            raise RuntimeError(result.stderr.strip() or "capture inspection failed")
        try:
            description = json.loads(result.stdout)
        except json.JSONDecodeError as error:
            raise RuntimeError(f"capture filter returned invalid inspection JSON: {error}") from error
        if not isinstance(description, dict):
            raise RuntimeError("capture inspection must be a JSON object")
        if not isinstance(description.get("calls"), list) or not isinstance(description.get("chunks"), list):
            raise RuntimeError("capture inspection is missing calls or chunks")
        self.validate_entries(description["calls"], "calls")
        self.validate_entries(description["chunks"], "chunks")
        return description

    @staticmethod
    def write_plan(path, calls, chunks):
        lines = ["aps5-frame-filter 1"]
        lines.extend(f"call {index}" for index in sorted(calls))
        lines.extend(f"chunk {index}" for index in sorted(chunks))
        path.write_text("\n".join(lines) + "\n", encoding="ascii")

    @staticmethod
    def reducible_chunks(description, removed_calls):
        retained_groups = {
            entry["memory_group"]
            for entry in description["calls"]
            if entry["index"] not in removed_calls and entry["memory_group"] is not None
        }
        removed_groups = {
            entry["memory_group"]
            for entry in description["calls"]
            if entry["index"] in removed_calls and entry["memory_group"] is not None
        } - retained_groups
        units = [
            entry["index"]
            for entry in description["chunks"]
            if entry["kind"] != "range"
            and not (entry["kind"] == "delta" and entry["group"] in removed_groups)
        ]
        return units, removed_groups

    def replay_once(self, capture, workdir):
        command = [*self.command(self.replay), str(capture)]
        command.extend(argument.replace("{trial_dir}", str(workdir)) for argument in self.args.replay_arg)
        stdout_path = workdir / "replay.stdout"
        stderr_path = workdir / "replay.stderr"
        try:
            with stdout_path.open("wb") as stdout, stderr_path.open("wb") as stderr:
                completed = subprocess.run(command, stdout=stdout, stderr=stderr, timeout=self.args.replay_timeout)
        except subprocess.TimeoutExpired:
            return False, f"replay timed out after {self.args.replay_timeout:g}s"
        stdout_text = stdout_path.read_text(encoding="utf-8", errors="replace")
        stderr_text = stderr_path.read_text(encoding="utf-8", errors="replace")
        matched = completed.returncode == self.args.expect_exit_code and self.pattern.search(stdout_text + "\n" + stderr_text) is not None
        return matched, f"exit={completed.returncode}, predicate={'matched' if matched else 'not matched'}"

    def predicate(self, calls, chunks):
        if self.trials >= self.args.max_trials:
            raise TrialLimitReached
        self.trials += 1
        with tempfile.TemporaryDirectory(prefix="aps5-frame-minimize-", dir=self.args.temp_dir) as temporary:
            root = Path(temporary)
            capture = root / "candidate"
            plan = root / "candidate.plan"
            self.write_plan(plan, calls, chunks)
            result = subprocess.run([*self.command(self.filter), "apply", str(self.capture), str(plan), str(capture)], capture_output=True, text=True, encoding="utf-8", errors="replace")
            if result.returncode != 0:
                raise RuntimeError(result.stderr.strip() or "capture filtering failed")
            for run in range(self.args.confirm_runs):
                workdir = root / f"replay-{run + 1}"
                workdir.mkdir()
                matched, detail = self.replay_once(capture, workdir)
                if not matched:
                    print(f"trial {self.trials}: rejected ({detail})", file=sys.stderr)
                    return False
            print(f"trial {self.trials}: predicate confirmed {self.args.confirm_runs} times", file=sys.stderr)
            return True

    def minimize(self, units, removed_calls, removed_chunks, is_call):
        current = list(units)
        granularity = 2
        while current:
            partitions = [current[index * len(current) // granularity:(index + 1) * len(current) // granularity] for index in range(granularity)]
            partitions = [part for part in partitions if part]
            changed = False
            try:
                for part in partitions:
                    part_set = set(part)
                    candidate = [unit for unit in current if unit not in part_set]
                    if len(candidate) == len(current):
                        continue
                    calls = removed_calls | (part_set if is_call else set())
                    chunks = removed_chunks | (part_set if not is_call else set())
                    if self.predicate(calls, chunks):
                        current = candidate
                        if is_call:
                            removed_calls.update(part)
                        else:
                            removed_chunks.update(part)
                        granularity = max(2, granularity - 1)
                        changed = True
                        break
                if changed:
                    continue
                for part in partitions:
                    candidate = list(part)
                    if len(candidate) == len(current):
                        continue
                    discarded = set(current) - set(candidate)
                    calls = removed_calls | (discarded if is_call else set())
                    chunks = removed_chunks | (discarded if not is_call else set())
                    if self.predicate(calls, chunks):
                        current = candidate
                        if is_call:
                            removed_calls.update(discarded)
                        else:
                            removed_chunks.update(discarded)
                        granularity = max(2, granularity - 1)
                        changed = True
                        break
            except TrialLimitReached:
                print("maximum trial count reached; the current result may not be minimal", file=sys.stderr)
                return current, removed_calls, removed_chunks, False
            if changed:
                continue
            if granularity >= len(current):
                break
            granularity = min(len(current), granularity * 2)
        return current, removed_calls, removed_chunks, True

    def materialize(self, calls, chunks, output):
        output.parent.mkdir(parents=True, exist_ok=True)
        with tempfile.NamedTemporaryFile(prefix="aps5-frame-plan-", suffix=".txt", dir=output.parent, delete=False) as file:
            plan = Path(file.name)
        try:
            self.write_plan(plan, calls, chunks)
            result = subprocess.run([*self.command(self.filter), "apply", str(self.capture), str(plan), str(output)], capture_output=True, text=True, encoding="utf-8", errors="replace")
        finally:
            plan.unlink(missing_ok=True)
        if result.returncode != 0:
            raise RuntimeError(result.stderr.strip() or "final capture filtering failed")
        self.output_created = True


def parse_args(argv=None):
    parser = argparse.ArgumentParser(description="Minimize a frame capture while a user-specified replay failure remains reproducible")
    parser.add_argument("--capture", required=True, type=Path)
    parser.add_argument("--replay", required=True, type=Path)
    parser.add_argument("--filter", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--match-regex", required=True, help="regular expression that identifies the failure in replay stdout or stderr")
    parser.add_argument("--expect-exit-code", required=True, type=int)
    parser.add_argument("--confirm-runs", type=int, default=2)
    parser.add_argument("--max-trials", type=int, default=1000)
    parser.add_argument("--replay-timeout", type=float, default=600)
    parser.add_argument("--temp-dir", type=Path)
    parser.add_argument("--replay-arg", action="append", default=[], help="argument passed through to agc_frame_replay; {trial_dir} expands to a fresh per-run directory")
    args = parser.parse_args(argv)
    if args.confirm_runs < 2:
        parser.error("--confirm-runs must be at least 2")
    if args.max_trials < 1:
        parser.error("--max-trials must be positive")
    if not math.isfinite(args.replay_timeout) or args.replay_timeout <= 0:
        parser.error("--replay-timeout must be a positive, finite number of seconds")
    if args.temp_dir is not None and not args.temp_dir.is_dir():
        parser.error("--temp-dir must name an existing directory")
    return args


def main(argv=None):
    args = parse_args(argv)
    if args.output.exists() or args.output.is_symlink():
        print(f"output already exists: {args.output}", file=sys.stderr)
        return 2
    try:
        pattern = re.compile(args.match_regex)
    except re.error as error:
        print(f"invalid --match-regex: {error}", file=sys.stderr)
        return 2
    minimizer = Minimizer(args, pattern)
    try:
        minimizer.output.parent.mkdir(parents=True, exist_ok=True)
        description = minimizer.inspect()
        call_units = [entry["index"] for entry in description["calls"] if entry["kind"] not in {"frame-start", "end"}]
        removed_calls = set()
        removed_chunks = set()
        for run in range(args.confirm_runs):
            workdir = Path(tempfile.mkdtemp(prefix="aps5-frame-baseline-", dir=args.temp_dir))
            try:
                matched, detail = minimizer.replay_once(minimizer.capture, workdir)
            finally:
                shutil.rmtree(workdir, ignore_errors=True)
            if not matched:
                print(f"baseline did not reproduce the requested predicate ({detail}); no output was created", file=sys.stderr)
                return 1
        print(f"baseline predicate confirmed {args.confirm_runs} times", file=sys.stderr)
        _, removed_calls, removed_chunks, calls_complete = minimizer.minimize(call_units, removed_calls, removed_chunks, True)
        chunk_units, removed_groups = minimizer.reducible_chunks(description, removed_calls)
        _, removed_calls, removed_chunks, chunks_complete = minimizer.minimize(chunk_units, removed_calls, removed_chunks, False)
        minimizer.materialize(removed_calls, removed_chunks, minimizer.output)
        auto_removed_chunks = {entry["index"] for entry in description["chunks"] if entry["kind"] == "delta" and entry["group"] in removed_groups}
        for run in range(args.confirm_runs):
            workdir = Path(tempfile.mkdtemp(prefix="aps5-frame-final-", dir=args.temp_dir))
            try:
                matched, detail = minimizer.replay_once(minimizer.output, workdir)
            finally:
                shutil.rmtree(workdir, ignore_errors=True)
            if not matched:
                print(f"final capture failed confirmation ({detail}): {minimizer.output}", file=sys.stderr)
                shutil.rmtree(minimizer.output, ignore_errors=True)
                minimizer.output_created = False
                return 1
        completeness = "minimality search completed" if calls_complete and chunks_complete else "trial limit reached; result is reduced but may not be minimal"
        print(f"wrote {minimizer.output}; removed {len(removed_calls)} calls and {len(removed_chunks | auto_removed_chunks)} chunks in {minimizer.trials} trials; final predicate confirmed {args.confirm_runs} times; {completeness}")
        return 0
    except (OSError, RuntimeError, subprocess.SubprocessError, KeyError, TypeError) as error:
        print(str(error), file=sys.stderr)
        if "minimizer" in locals() and minimizer.output_created:
            shutil.rmtree(minimizer.output, ignore_errors=True)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
