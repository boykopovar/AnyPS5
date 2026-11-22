import argparse
import contextlib
import json
import math
import os
from pathlib import Path
import re
import shutil
import signal
import subprocess
import sys
import threading
import time

import frame_replay_report as evidence
from frame_capture_minimize import Minimizer
import vulkan_validation_gate as processes


ARMS = ("baseline", "candidate")
FORCED = {"ANYPS5_REQUIRE_VULKAN": "1", "ANYPS5_HOST_REPORT": "1"}
LIMITS = {"trials": (1, 20, 3), "warmups": (0, 5, 1), "timeout": (0.1, 3600, 120),
          "log_mib": (1, 128, 16)}
NOTES = [
    "These are observations of captured driver work, not live-title FPS or proof of capture fidelity.",
    "Each trial is a fresh process with --strict --no-synthesize --repeat 1; in-process caches start fresh.",
    "Application cache seeds are private to each arm. Identical cache bytes do not establish equal warmness across builds.",
    "Driver caches, GPU clocks, scheduling and physical-device identity are not controlled or verified.",
    "Runtime file hashes identify staged bytes, not a clean source build or all libraries actually loaded by the OS.",
]


def _relative_file(root, value):
    if not isinstance(value, str) or not value or "\\" in value or ":" in value:
        raise ValueError("tool paths must be nonempty relative POSIX paths inside runtime")
    path = Path(value)
    if path.is_absolute() or ".." in path.parts or str(path) == ".":
        raise ValueError("tool paths must stay inside runtime")
    result = root / path
    if not result.is_file():
        raise ValueError(f"tool is not a regular file: {result}")
    return result


def _environment(value, shared=False):
    if not isinstance(value, dict) or len(value) > 128:
        raise ValueError("environment must be an object with at most 128 entries")
    for key, text in value.items():
        if not isinstance(key, str) or not re.fullmatch(r"(?:APS5_|ANYPS5_|VK_)[A-Z0-9_]+", key):
            raise ValueError(f"unsupported environment key: {key!r}")
        if not isinstance(text, str) or len(text) > 4096 or "\0" in text:
            raise ValueError(f"invalid environment value: {key}")
        if key in FORCED or key == "ANYPS5_SHADER_CACHE_DIR" or key.startswith("APS5_FRAME_CAPTURE"):
            raise ValueError(f"environment key is controlled by the runner: {key}")
        if not shared and (key.startswith("VK_") or key == "ANYPS5_GPU"):
            raise ValueError(f"device selection must be shared by both arms: {key}")
    return value


def _requires_validation(environment):
    layers = re.split(r"[:;]", environment.get("VK_INSTANCE_LAYERS", ""))
    return processes.FORCED["VK_INSTANCE_LAYERS"] in layers


def validate_replay_recipe(value, directory):
    if not isinstance(value, dict) or value.get("format") != "aps5-frame-replay-recipe/1":
        raise ValueError("expected format aps5-frame-replay-recipe/1")
    if set(value) - {"format", "capture", "arms", "environment", *LIMITS}:
        raise ValueError("unknown recipe fields")
    result = dict(value)
    capture = value.get("capture")
    if not isinstance(capture, str) or not capture:
        raise ValueError("capture must be a directory path")
    result["capture"] = str((directory / capture).absolute())
    if not Path(result["capture"]).is_dir():
        raise ValueError("capture directory does not exist")
    arms = value.get("arms")
    if not isinstance(arms, dict) or set(arms) != set(ARMS):
        raise ValueError("arms must contain exactly baseline and candidate")
    result["arms"] = {}
    for name in ARMS:
        arm = arms[name]
        if not isinstance(arm, dict) or set(arm) - {"runtime", "replay", "inspect", "commit", "environment", "library_dirs"}:
            raise ValueError(f"invalid arm: {name}")
        if not isinstance(arm.get("runtime"), str) or not arm["runtime"]:
            raise ValueError(f"{name}: runtime must be a directory")
        root = (directory / arm["runtime"]).absolute()
        if not root.is_dir():
            raise ValueError(f"{name}: runtime directory does not exist")
        if not isinstance(arm.get("commit"), str) or not re.fullmatch(r"[0-9a-f]{40}", arm["commit"]):
            raise ValueError(f"{name}: commit must be a full lowercase Git ID")
        for key in ("replay", "inspect"):
            _relative_file(root, arm.get(key))
        directories = arm.get("library_dirs", ["."])
        if not isinstance(directories, list) or not 1 <= len(directories) <= 16:
            raise ValueError("library_dirs must contain between 1 and 16 relative directories")
        for relative in directories:
            if not isinstance(relative, str) or not relative or "\\" in relative or ":" in relative:
                raise ValueError("invalid library directory")
            path = Path(relative)
            if path.is_absolute() or ".." in path.parts or not (root / path).is_dir():
                raise ValueError("library directory must exist inside runtime")
        result["arms"][name] = {**arm, "runtime": str(root), "library_dirs": directories,
                                "environment": _environment(arm.get("environment", {}))}
    result["environment"] = _environment(value.get("environment", {}), shared=True)
    for key, (low, high, default) in LIMITS.items():
        number = value.get(key, default)
        kind = (int, float) if key == "timeout" else (int,)
        if type(number) not in kind or not low <= number <= high or not math.isfinite(number):
            raise ValueError(f"{key} must be between {low} and {high}")
        result[key] = number
    return result


def _write_json(path, value):
    data = json.dumps(value, ensure_ascii=False, indent=2, allow_nan=False) + "\n"
    if len(data.encode("utf-8")) > evidence.MAX_JSON_BYTES:
        raise ValueError("comparison metadata exceeds its JSON byte limit")
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_text(data, encoding="utf-8")
    temporary.replace(path)


def _read_json(path):
    with path.open("rb") as stream:
        return evidence.load_replay_json(stream.read(evidence.MAX_JSON_BYTES + 1))


def _copy_tree(source, target, manifest):
    if shutil.disk_usage(target.parent).free < sum(entry["bytes"] for entry in manifest) + 128 * 1024 * 1024:
        raise ValueError("insufficient disk space to stage input or cache while retaining evidence")
    target.mkdir()
    for entry in manifest:
        relative = entry["path"]
        destination = target / relative
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source / relative, destination, follow_symlinks=False)
    evidence.verify_replay_bundle(target, manifest)


def stage_replay_runtime(source, target, manifest):
    _copy_tree(source, target, manifest)
    return target


def _trial_environment(recipe, arm, runtime, cache):
    environment = {key: value for key, value in os.environ.items()
                   if not key.startswith(("APS5_", "ANYPS5_", "VK_", "LD_", "DYLD_"))}
    libraries = [str(runtime / relative) for relative in arm["library_dirs"]]
    if os.name == "nt":
        system = Path(os.environ.get("SystemRoot", "C:/Windows"))
        environment["PATH"] = os.pathsep.join([*libraries, str(system / "System32"), str(system)])
    else:
        environment["LD_LIBRARY_PATH"] = os.pathsep.join(libraries)
    environment.update(recipe["environment"])
    environment.update(arm["environment"])
    environment.update(FORCED)
    environment["ANYPS5_SHADER_CACHE_DIR"] = str(cache)
    if _requires_validation(recipe["environment"]):
        flags = environment.get("VK_LOADER_DEBUG", "").split(",")
        if "all" not in flags and "layer" not in flags:
            environment["VK_LOADER_DEBUG"] = ",".join(filter(None, (*flags, "layer")))
    return environment


def execute_replay_trial(command, directory, environment, timeout, log_bytes):
    directory.mkdir(parents=True)
    exceeded = threading.Event()
    errors = []

    def pump(stream, path):
        try:
            with path.open("wb") as output:
                total = 0
                while block := stream.read1(65536):
                    remaining = max(0, log_bytes - total)
                    output.write(block[:remaining])
                    total += len(block)
                    if total > log_bytes:
                        exceeded.set()
                        break
        except BaseException as error:
            errors.append(str(error))
            exceeded.set()
        finally:
            stream.close()

    process = None
    job = None
    threads = []
    timed_out = False
    cleanup_failed = False
    interrupted = False
    started = time.monotonic()
    try:
        if os.name == "nt":
            processes.guard()
        process = subprocess.Popen(command, cwd=directory, env=environment, stdin=subprocess.DEVNULL,
                                   stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                   **({"creationflags": processes.CREATE_SUSPENDED} if os.name == "nt"
                                      else {"start_new_session": True}))
        if os.name == "nt":
            job = processes.contain(process.pid)
        for stream, name in ((process.stdout, "stdout.txt"), (process.stderr, "stderr.txt")):
            thread = threading.Thread(target=pump, args=(stream, directory / name), daemon=True)
            thread.start()
            threads.append(thread)
        while process.poll() is None and not exceeded.is_set():
            if time.monotonic() - started >= timeout:
                timed_out = True
                break
            time.sleep(0.025)
    except (OSError, processes.GateError) as error:
        errors.append(str(error))
    except KeyboardInterrupt:
        interrupted = True
        errors.append("interrupted")
    finally:
        if process is not None:
            try:
                if job is not None:
                    processes.drain(job, process, strict=True)
                else:
                    if os.name == "nt":
                        with contextlib.suppress(OSError):
                            process.kill()
                    else:
                        with contextlib.suppress(ProcessLookupError):
                            os.killpg(process.pid, signal.SIGKILL)
                    process.wait(timeout=10)
            except (OSError, subprocess.SubprocessError, processes.GateError) as error:
                errors.append(f"process cleanup failed: {error}")
                cleanup_failed = True
            if not threads:
                process.stdout.close()
                process.stderr.close()
        for thread in threads:
            thread.join(timeout=10)
            if thread.is_alive():
                errors.append("output reader survived process cleanup")
                cleanup_failed = True
    result = {"command": command, "returncode": process.returncode if process else None, "timed_out": timed_out,
            "log_limit": exceeded.is_set(), "io_errors": errors, "cleanup_failed": cleanup_failed,
            "interrupted": interrupted,
            "wall_s": time.monotonic() - started}
    _write_json(directory / "process.json", result)
    return result


def _read_log(path):
    if not path.exists():
        return ""
    with path.open("rb") as stream:
        return stream.read(evidence.MAX_LOG_BYTES + 1).decode("utf-8", errors="replace")


def _assess_replay(directory, commit, requires_validation):
    stdout = _read_log(directory / "stdout.txt")
    stderr = _read_log(directory / "stderr.txt")
    assessment = evidence.parse_replay_result(stdout, stderr, commit)
    if requires_validation:
        logs = (stdout, stderr)
        if any(processes.MISSING_LAYER in log for log in logs):
            assessment["issues"].append("Vulkan loader reported a requested layer missing")
        if not any(processes.INSERTED_LAYER in log for log in logs):
            assessment["issues"].append("Khronos validation layer insertion was not recorded")
    return assessment


def _inspect_replay(recipe, arm, runtime, capture, directory):
    cache = directory.parent / (directory.name + "-cache")
    cache.mkdir()
    command = [str(runtime / arm["inspect"]), "inspect", str(capture)]
    result = execute_replay_trial(command, directory, _trial_environment(recipe, arm, runtime, cache),
                                  recipe["timeout"], recipe["log_mib"] * 1024 * 1024)
    if result.get("interrupted"):
        raise KeyboardInterrupt
    if result["returncode"] != 0 or result["timed_out"] or result["log_limit"] or result["io_errors"]:
        raise ValueError("capture inspection failed; see " + str(directory))
    description = evidence.load_replay_json(_read_log(directory / "stdout.txt"))
    if not isinstance(description, dict) or set(description) != {"calls", "chunks"}:
        raise ValueError("capture inspection must contain calls and chunks")
    for key in ("calls", "chunks"):
        if not isinstance(description[key], list) or not description[key]:
            raise ValueError("capture inspection has no " + key)
        Minimizer.validate_entries(description[key], key)
    return result


def _verify_inputs(report):
    for name, manifest in report["inputs"].items():
        evidence.verify_replay_bundle(Path(report["input_paths"][name]), manifest)


def _comparison_checks(report):
    contexts = []
    work = []
    for run in report["runs"]:
        if run["phase"] != "trial" or run["assessment"]["issues"]:
            continue
        host = run["assessment"]["host"]
        native = run["assessment"]["native"]
        contexts.append({section: {key: host[section].get(key) for key in keys} for section, keys in (
            ("system", ("os", "cpu", "cpu_threads")),
            ("gpu", ("device", "vendor_id", "device_id", "driver", "driver_info", "driver_version_raw", "vulkan_api")))})
        work.append({key: native[key] for key in ("frames", "submits", "threads", "frames_compared")})
    if contexts and any(context != contexts[0] for context in contexts[1:]):
        report["issues"].append("observed host/GPU/driver context differs between trials")
    if work and any(counts != work[0] for counts in work[1:]):
        report["issues"].append("reported frame/submission/thread/reference counts differ between trials")


def _capture_metadata(capture, report):
    metadata = _read_json(capture / "capture.json")
    if not isinstance(metadata, dict) or metadata.get("magic") != "aps5-frame-capture 1":
        raise ValueError("unsupported capture metadata")
    warnings = metadata.get("warnings")
    if not isinstance(warnings, list) or len(warnings) > 1000 or any(not isinstance(warning, str) or len(warning) > 4096 for warning in warnings):
        raise ValueError("invalid capture warnings")
    for key in ("frames", "unreadable_bytes", "host_only_sample_bytes"):
        if type(metadata.get(key)) is not int or not 0 <= metadata[key] <= (1 << 64) - 1:
            raise ValueError(f"invalid capture metadata {key}")
    report["capture_metadata"] = metadata
    if not metadata["frames"]:
        report["issues"].append("capture records no frames")
    if metadata["unreadable_bytes"] or metadata["host_only_sample_bytes"]:
        report["issues"].append("capture metadata records unreadable or omitted host-only memory")
    report["issues"].extend("capture warning: " + warning for warning in warnings)
    if report["issues"]:
        raise ValueError("capture metadata reports incomplete evidence")


def _run_one(recipe, report, output, arm_name, phase, iteration, runtime, capture, seed):
    name = f"{phase}-{iteration:02d}-{arm_name}"
    root = output / "runs" / name
    cache = output / "caches" / name
    cache.parent.mkdir(exist_ok=True)
    seed_manifest = evidence.replay_tree_manifest(seed)
    _copy_tree(seed, cache, seed_manifest)
    arm = recipe["arms"][arm_name]
    command = [str(runtime / arm["replay"]), str(capture), "--strict", "--no-synthesize", "--repeat", "1"]
    result = execute_replay_trial(command, root, _trial_environment(recipe, arm, runtime, cache),
                                  recipe["timeout"], recipe["log_mib"] * 1024 * 1024)
    assessment = _assess_replay(root, arm["commit"], _requires_validation(recipe["environment"]))
    for key in ("timed_out", "log_limit"):
        if result[key]:
            assessment["issues"].append(key)
    if result["returncode"] != 0:
        assessment["issues"].append(f"process exit {result['returncode']}")
    assessment["issues"].extend(result["io_errors"])
    run = {**result, "arm": arm_name, "phase": phase, "iteration": iteration,
           "path": root.relative_to(output).as_posix(), "seed": seed_manifest, "assessment": assessment}
    report["runs"].append(run)
    _write_json(output / "report.json", evidence.replay_comparison_report(report))
    if result.get("interrupted"):
        raise KeyboardInterrupt
    if result.get("cleanup_failed"):
        raise RuntimeError("native process cleanup failed; experiment aborted")
    _verify_inputs(report)
    evidence.verify_replay_bundle(runtime, report["inputs"][arm_name])
    evidence.verify_replay_bundle(capture, report["inputs"]["capture"])
    return cache, not assessment["issues"]


def run_replay_comparison(recipe, output):
    paths = {"capture": Path(recipe["capture"]), **{arm: Path(recipe["arms"][arm]["runtime"]) for arm in ARMS}}
    output = output.absolute()
    for path in paths.values():
        left, right = path.resolve(), output.resolve()
        if left == right or left in right.parents or right in left.parents:
            raise ValueError("output must not overlap any capture or runtime input")
    manifests = {name: evidence.replay_tree_manifest(path) for name, path in paths.items()}
    output.mkdir(parents=True, exist_ok=False)
    report = {"format": "aps5-frame-replay-comparison/1", "recipe": recipe, "input_paths": {name: str(path) for name, path in paths.items()},
              "inputs": manifests, "runs": [], "issues": [], "unknowns": list(NOTES), "complete": False}
    _write_json(output / "report.json", report)
    try:
        capture = output / "capture"
        _copy_tree(paths["capture"], capture, manifests["capture"])
        _capture_metadata(capture, report)
        runtimes = {}
        seeds = {}
        (output / "runtime").mkdir()
        (output / "seeds").mkdir()
        for arm in ARMS:
            runtime = stage_replay_runtime(paths[arm], output / "runtime" / arm, manifests[arm])
            runtimes[arm] = runtime
            _inspect_replay(recipe, recipe["arms"][arm], runtime, capture, output / ("inspect-" + arm))
            seed = output / "seeds" / arm
            seed.mkdir()
            for iteration in range(1, recipe["warmups"] + 1):
                warmed, good = _run_one(recipe, report, output, arm, "warmup", iteration, runtime, capture, seed)
                if not good:
                    raise ValueError(f"{arm}: warmup failed; all evidence retained")
                if not seed.resolve().is_relative_to(output.resolve() / "seeds"):
                    raise ValueError("cache seed escaped its owned directory")
                evidence.replay_tree_manifest(seed)
                shutil.rmtree(seed)
                _copy_tree(warmed, seed, evidence.replay_tree_manifest(warmed))
            seeds[arm] = seed
        for iteration in range(1, recipe["trials"] + 1):
            order = ARMS if iteration % 2 else tuple(reversed(ARMS))
            for arm in order:
                _run_one(recipe, report, output, arm, "trial", iteration, runtimes[arm], capture, seeds[arm])
        _verify_inputs(report)
        _comparison_checks(report)
        report["complete"] = True
    except KeyboardInterrupt:
        report["issues"].append("interrupted")
    except (ValueError, OSError, RuntimeError, subprocess.SubprocessError) as error:
        report["issues"].append(str(error))
    finally:
        _write_json(output / "comparison.json", report)
        try:
            report["artifacts"] = evidence.replay_tree_manifest(output, exclude=("report.json", "report.md"))
        except (ValueError, OSError) as error:
            report["issues"].append(f"cannot seal evidence: {error}")
            report["complete"] = False
        report = evidence.replay_comparison_report(report)
        _write_json(output / "report.json", report)
        (output / "report.md").write_text(evidence.replay_comparison_report(report, markdown=True), encoding="utf-8")
    return report


def _offline_report(directory):
    report = _read_json(directory / "report.json")
    if not isinstance(report, dict) or report.get("format") != "aps5-frame-replay-comparison/1" or "artifacts" not in report:
        raise ValueError("not a completed replay evidence bundle")
    evidence.verify_replay_bundle(directory, report["artifacts"], exclude=("report.json", "report.md"))
    original = _read_json(directory / "comparison.json")
    if not isinstance(original, dict) or original.get("format") != "aps5-frame-replay-comparison/1":
        raise ValueError("invalid original comparison evidence")
    if not isinstance(original.get("runs"), list) or len(original["runs"]) > 50:
        raise ValueError("invalid original comparison runs")
    if type(original.get("complete")) is not bool or not isinstance(original.get("recipe"), dict):
        raise ValueError("invalid original comparison state")
    environment = _environment(original["recipe"].get("environment", {}), shared=True)
    requires_validation = _requires_validation(environment)
    original["artifacts"] = report["artifacts"]
    for run in original["runs"]:
        if not isinstance(run, dict) or run.get("arm") not in ARMS or run.get("phase") not in ("warmup", "trial"):
            raise ValueError("invalid run identity")
        if type(run.get("iteration")) is not int or not 1 <= run["iteration"] <= 20:
            raise ValueError("invalid run iteration")
        if run.get("path") != f"runs/{run['phase']}-{run['iteration']:02d}-{run['arm']}":
            raise ValueError("invalid run evidence path")
        root = directory / run["path"]
        process = _read_json(root / "process.json")
        if not isinstance(process, dict) or type(process.get("timed_out")) is not bool or type(process.get("log_limit")) is not bool:
            raise ValueError("invalid process evidence")
        if process.get("returncode") is not None and type(process["returncode"]) is not int:
            raise ValueError("invalid process return code")
        if not isinstance(process.get("io_errors"), list) or any(not isinstance(error, str) for error in process["io_errors"]):
            raise ValueError("invalid process I/O evidence")
        run.update(process)
        arms = original["recipe"].get("arms")
        if not isinstance(arms, dict) or not isinstance(arms.get(run["arm"]), dict):
            raise ValueError("invalid original arm recipe")
        commit = arms[run["arm"]].get("commit")
        if not isinstance(commit, str) or not re.fullmatch(r"[0-9a-f]{40}", commit):
            raise ValueError("invalid expected runtime commit")
        native = _assess_replay(root, commit, requires_validation)
        for key in ("timed_out", "log_limit"):
            if run[key]:
                native["issues"].append(key)
        if run["returncode"] != 0:
            native["issues"].append(f"process exit {run['returncode']}")
        native["issues"].extend(run["io_errors"])
        run["assessment"] = native
    _comparison_checks(original)
    return evidence.replay_comparison_report(original)


def main(argv=None):
    parser = argparse.ArgumentParser(description="Run and compare pinned native frame replays; retain limitations and every attempt.")
    commands = parser.add_subparsers(dest="command", required=True)
    run = commands.add_parser("run")
    run.add_argument("recipe", type=Path)
    run.add_argument("--output", type=Path, required=True)
    view = commands.add_parser("report")
    view.add_argument("bundle", type=Path)
    view.add_argument("--json", action="store_true")
    args = parser.parse_args(argv)
    try:
        if args.command == "run":
            recipe = validate_replay_recipe(_read_json(args.recipe), args.recipe.absolute().parent)
            report = run_replay_comparison(recipe, args.output)
            print(str(args.output / "report.md"))
        else:
            report = _offline_report(args.bundle)
            print(json.dumps(report, indent=2, ensure_ascii=False, allow_nan=False) if args.json
                  else evidence.replay_comparison_report(report, markdown=True))
        problems = report.get("issues", []) or any(run["assessment"]["issues"] for run in report["runs"])
        return 130 if "interrupted" in report.get("issues", []) else 2 if problems or not report.get("complete") else 0
    except (ValueError, OSError, RuntimeError, subprocess.SubprocessError) as error:
        print(f"frame_replay_compare: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
