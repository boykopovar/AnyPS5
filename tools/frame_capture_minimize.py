import argparse
import json
import os
import re
import shutil
import signal
import subprocess
import sys
import tempfile
import time
from pathlib import Path

import vulkan_validation_gate as command_lifecycle


REPLAY_DRAIN_SECONDS = 10


class ReplayCleanupError(RuntimeError):
    pass


if os.name == "nt":
    ctypes = command_lifecycle.ctypes
    wintypes = command_lifecycle.wintypes

    class CompletionPort(ctypes.Structure):
        _fields_ = [("key", ctypes.c_void_p), ("port", wintypes.HANDLE)]

    for name, restype, *argtypes in (
            ("CreateIoCompletionPort", wintypes.HANDLE, wintypes.HANDLE, wintypes.HANDLE, ctypes.c_size_t, wintypes.DWORD),
            ("GetQueuedCompletionStatus", wintypes.BOOL, wintypes.HANDLE, ctypes.POINTER(wintypes.DWORD),
             ctypes.POINTER(ctypes.c_size_t), ctypes.POINTER(ctypes.c_void_p), wintypes.DWORD),
            ("IsProcessInJob", wintypes.BOOL, wintypes.HANDLE, wintypes.HANDLE, ctypes.POINTER(wintypes.BOOL)),
            ("WaitForSingleObject", wintypes.DWORD, wintypes.HANDLE, wintypes.DWORD)):
        function = getattr(command_lifecycle.KERNEL32, name)
        function.restype, function.argtypes = restype, argtypes

    class ReplayJob:
        def __init__(self, process):
            self.process = process
            self.port = None
            self.job = None
            self.observed = 1
            self.root_notified = False
            self.handles = []

        def prepare(self, job):
            self.job = job
            self.port = command_lifecycle.win32("CreateIoCompletionPort", command_lifecycle.INVALID_HANDLE_VALUE, None, 0, 1)
            association = CompletionPort(1, self.port)
            command_lifecycle.win32("SetInformationJobObject", job, 7, ctypes.byref(association), ctypes.sizeof(association))

        def collect(self):
            for _ in range(1024):
                message, key, pid = wintypes.DWORD(), ctypes.c_size_t(), ctypes.c_void_p()
                if not command_lifecycle.KERNEL32.GetQueuedCompletionStatus(self.port, ctypes.byref(message), ctypes.byref(key), ctypes.byref(pid), 0):
                    if ctypes.get_last_error() == 258:
                        return
                    raise ctypes.WinError(ctypes.get_last_error())
                if key.value != 1:
                    raise RuntimeError("replay job returned an unknown completion key")
                if message.value != 6:
                    continue
                if pid.value == self.process.pid and not self.root_notified:
                    self.root_notified = True
                    continue
                handle = command_lifecycle.KERNEL32.OpenProcess(0x101000, False, pid.value)
                if not handle:
                    if ctypes.get_last_error() == 87:
                        self.observed += 1
                        continue
                    raise ctypes.WinError(ctypes.get_last_error())
                try:
                    member = wintypes.BOOL()
                    command_lifecycle.win32("IsProcessInJob", handle, self.job, ctypes.byref(member))
                    if member.value:
                        self.handles.append(handle)
                        handle = None
                    elif command_lifecycle.KERNEL32.WaitForSingleObject(handle, 0) != 0:
                        raise RuntimeError("replay notification no longer identifies a terminated job member")
                    self.observed += 1
                finally:
                    command_lifecycle.close(handle)

        def close(self):
            for handle in self.handles:
                command_lifecycle.close(handle)
            command_lifecycle.close(self.port)


def drain_replay_job(job, process, tracker):
    deadline = time.monotonic() + REPLAY_DRAIN_SECONDS
    try:
        limits = command_lifecycle.Limits(flags=command_lifecycle.KILL_ON_JOB_CLOSE | 8, active_limit=0)
        try:
            command_lifecycle.win32("SetInformationJobObject", job, command_lifecycle.LIMITS,
                                    command_lifecycle.ctypes.byref(limits), command_lifecycle.ctypes.sizeof(limits))
            tracker.collect()
        finally:
            command_lifecycle.win32("TerminateJobObject", job, 1)
        waited = 0
        while True:
            tracker.collect()
            for handle in tracker.handles[waited:]:
                result = command_lifecycle.KERNEL32.WaitForSingleObject(handle, max(0, int((deadline - time.monotonic()) * 1000)))
                if result != 0:
                    raise RuntimeError(f"replay descendant termination is unconfirmed (wait result {result})")
                waited += 1
            process.wait(timeout=max(0.0, deadline - time.monotonic()))
            accounting = command_lifecycle.Accounting()
            command_lifecycle.win32("QueryInformationJobObject", job, command_lifecycle.ACCOUNTING,
                                    command_lifecycle.ctypes.byref(accounting), command_lifecycle.ctypes.sizeof(accounting), None)
            if accounting.total == tracker.observed and accounting.active == 0:
                return
            if accounting.total < tracker.observed or time.monotonic() >= deadline:
                raise RuntimeError("replay job membership or termination is unconfirmed")
            time.sleep(0.02)
    except BaseException as error:
        raise ReplayCleanupError(f"cannot confirm replay process job cleanup: {error}") from error
    finally:
        command_lifecycle.close(job)
        tracker.close()


def wait_replay_job(process, tracker, timeout):
    deadline = time.monotonic() + timeout
    while True:
        tracker.collect()
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            raise subprocess.TimeoutExpired(process.args, timeout)
        try:
            return process.wait(timeout=min(remaining, 0.25))
        except subprocess.TimeoutExpired:
            pass


def drain_replay_group(process):
    deadline = time.monotonic() + REPLAY_DRAIN_SECONDS
    try:
        try:
            os.killpg(process.pid, signal.SIGKILL)
        except ProcessLookupError:
            pass
        except BaseException:
            try:
                process.kill()
            except OSError:
                pass
            try:
                process.wait(timeout=max(0.0, deadline - time.monotonic()))
            except BaseException:
                pass
            raise
        process.wait(timeout=max(0.0, deadline - time.monotonic()))
        while True:
            try:
                os.killpg(process.pid, 0)
            except ProcessLookupError:
                return
            except PermissionError:
                if sys.platform != "darwin":
                    raise
            if time.monotonic() >= deadline:
                raise RuntimeError("replay process group remains after termination")
            time.sleep(0.02)
    except BaseException as error:
        raise ReplayCleanupError(f"cannot confirm replay process group cleanup: {error}") from error


def run_replay(command, stdout, stderr, timeout):
    if sys.platform in ("cygwin", "msys") or os.name not in ("nt", "posix"):
        raise RuntimeError("use native Windows Python or a POSIX host to contain replay processes")
    try:
        command_lifecycle.positive(timeout)
    except argparse.ArgumentTypeError as error:
        raise RuntimeError("replay timeout must be finite and positive") from error
    if os.name == "nt":
        command_lifecycle.guard()
        process = subprocess.Popen(command, stdout=stdout, stderr=stderr,
                                   creationflags=command_lifecycle.CREATE_SUSPENDED)
        tracker = ReplayJob(process)
        try:
            job = command_lifecycle.contain(process.pid, tracker.prepare)
        except BaseException as error:
            tracker.close()
            try:
                try:
                    process.kill()
                except OSError:
                    pass
                process.wait(timeout=REPLAY_DRAIN_SECONDS)
            except BaseException as cleanup:
                raise ReplayCleanupError(f"replay containment failed and its process could not be reaped: {cleanup}") from error
            raise ReplayCleanupError(f"replay containment failed; descendant cleanup is unconfirmed: {error}") from error
        try:
            return wait_replay_job(process, tracker, timeout)
        finally:
            drain_replay_job(job, process, tracker)
    process = subprocess.Popen(command, stdout=stdout, stderr=stderr,
                               start_new_session=True)
    try:
        return process.wait(timeout=timeout)
    finally:
        drain_replay_group(process)


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
        self.cleanup_failed = False

    def cleanup(self, path, ignore_errors=False):
        if self.cleanup_failed:
            print(f"replay cleanup is unconfirmed; retaining {path}", file=sys.stderr)
            return
        shutil.rmtree(path, ignore_errors=ignore_errors)

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
            if label == "calls":
                group = entry.get("memory_group")
                if kind == "memory":
                    valid_group = type(group) is int and group >= 0
                else:
                    valid_group = "memory_group" in entry and group is None
                if not valid_group:
                    raise RuntimeError(f"capture inspection calls[{position}] has an invalid memory group")
            if label == "chunks":
                group = entry.get("group")
                if isinstance(group, bool) or not isinstance(group, int) or group < 0:
                    raise RuntimeError(f"capture inspection chunks[{position}] has an invalid group")

    def inspect(self):
        from performance_compare import unique_json_object

        result = subprocess.run([*self.command(self.filter), "inspect", str(self.capture)], capture_output=True, text=True, encoding="utf-8", errors="replace")
        if result.returncode != 0:
            raise RuntimeError(result.stderr.strip() or "capture inspection failed")
        try:
            description = json.loads(result.stdout, object_pairs_hook=unique_json_object)
        except ValueError as error:
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
                try:
                    returncode = run_replay(command, stdout, stderr, self.args.replay_timeout)
                except ReplayCleanupError:
                    self.cleanup_failed = True
                    raise
        except subprocess.TimeoutExpired:
            return False, f"replay timed out after {self.args.replay_timeout:g}s"
        stdout_text = stdout_path.read_text(encoding="utf-8", errors="replace")
        stderr_text = stderr_path.read_text(encoding="utf-8", errors="replace")
        matched = returncode == self.args.expect_exit_code and self.pattern.search(stdout_text + "\n" + stderr_text) is not None
        return matched, f"exit={returncode}, predicate={'matched' if matched else 'not matched'}"

    def predicate(self, calls, chunks):
        if self.trials >= self.args.max_trials:
            raise TrialLimitReached
        self.trials += 1
        root = Path(tempfile.mkdtemp(prefix="aps5-frame-minimize-", dir=self.args.temp_dir))
        try:
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
        finally:
            self.cleanup(root)

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
                for part in partitions if len(partitions) > 2 else ():
                    candidate = list(part)
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
        with tempfile.TemporaryDirectory(prefix="aps5-frame-output-", dir=output.parent) as temporary:
            root = Path(temporary)
            plan = root / "candidate.plan"
            candidate = root / "capture"
            self.write_plan(plan, calls, chunks)
            result = subprocess.run([*self.command(self.filter), "apply", str(self.capture), str(plan), str(candidate)], capture_output=True, text=True, encoding="utf-8", errors="replace")
            if result.returncode != 0:
                raise RuntimeError(result.stderr.strip() or "final capture filtering failed")
            if candidate.is_symlink() or not candidate.is_dir():
                raise RuntimeError("final capture filter did not create a capture directory")
            output.mkdir()
            self.output_created = True
            try:
                for entry in candidate.iterdir():
                    entry.replace(output / entry.name)
            except BaseException:
                shutil.rmtree(output)
                self.output_created = False
                raise


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
    if args.replay_timeout <= 0:
        parser.error("--replay-timeout must be positive")
    if args.temp_dir is not None and not args.temp_dir.is_dir():
        parser.error("--temp-dir must name an existing directory")
    return args


def main(argv=None):
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, "reconfigure"):
            stream.reconfigure(encoding="utf-8", errors="backslashreplace")
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
                minimizer.cleanup(workdir, ignore_errors=True)
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
                minimizer.cleanup(workdir, ignore_errors=True)
            if not matched:
                print(f"final capture failed confirmation ({detail}): {minimizer.output}", file=sys.stderr)
                shutil.rmtree(minimizer.output, ignore_errors=True)
                minimizer.output_created = False
                return 1
        completeness = "minimality search completed" if calls_complete and chunks_complete else "trial limit reached; result is reduced but may not be minimal"
        print(f"wrote {minimizer.output}; removed {len(removed_calls)} calls and {len(removed_chunks | auto_removed_chunks)} chunks in {minimizer.trials} trials; final predicate confirmed {args.confirm_runs} times; {completeness}")
        return 0
    except KeyboardInterrupt:
        print("frame capture minimization interrupted", file=sys.stderr)
        if minimizer.output_created:
            minimizer.cleanup(minimizer.output, ignore_errors=True)
        return 130
    except (OSError, RuntimeError, subprocess.SubprocessError, KeyError, TypeError) as error:
        print(str(error), file=sys.stderr)
        if "minimizer" in locals() and minimizer.output_created:
            minimizer.cleanup(minimizer.output, ignore_errors=True)
        return 2


if __name__ == "__main__":
    sys.path.insert(0, str(Path(__file__).resolve().parent))
    raise SystemExit(main())
