import argparse
import importlib.util
import json
import math
import os
from pathlib import Path
import re
import subprocess
import sys
import time

REGISTRATION_SPEC = importlib.util.spec_from_file_location("check_test_registration", Path(__file__).with_name("check_test_registration.py"))
REGISTRATION = importlib.util.module_from_spec(REGISTRATION_SPEC)
REGISTRATION_SPEC.loader.exec_module(REGISTRATION)


def test_commands(root, names):
    found, visited, pending = {}, set(), [Path(root).resolve()]
    while pending:
        directory = pending.pop()
        file = directory / "CTestTestfile.cmake"
        if directory in visited or not file.is_file():
            continue
        visited.add(directory)
        try:
            commands = list(REGISTRATION.commands(file.read_text(encoding="utf-8-sig")))
        except (OSError, UnicodeError, ValueError):
            continue
        for command, arguments in commands:
            if command == "subdirs":
                pending.extend(directory / argument for argument in arguments)
            elif command == "add_test" and arguments and arguments[0] in names and len(arguments) > 1:
                found.setdefault(arguments[0], set()).add(tuple(arguments[1:]))
    return found


def main():
    parser = argparse.ArgumentParser(description="Run graphics correctness and synthetic latency gates without a game")
    parser.add_argument("--build", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--ctest", default="ctest")
    parser.add_argument("--cmake", default="cmake")
    parser.add_argument("--prepare", action="store_true", help="Build the selected test executables before running")
    parser.add_argument("--performance", action="store_true", help="Measure without validation instrumentation; run correctness separately first")
    parser.add_argument("--runs", type=int, default=3)
    parser.add_argument("--budget-ms", type=float, default=1000 / 60)
    parser.add_argument("--max-ms", type=float, default=1000 / 30)
    args = parser.parse_args()
    if args.runs < 1 or not math.isfinite(args.budget_ms) or not math.isfinite(args.max_ms) or args.budget_ms <= 0 or args.max_ms <= 0:
        parser.error("runs and budget must be positive")
    args.output.mkdir(parents=True, exist_ok=True)
    env = os.environ.copy()
    env["ANYPS5_REQUIRE_VULKAN"] = "1"
    env["ANYPS5_NO_SHADER_CACHE"] = "1"
    if args.performance:
        env.pop("VK_INSTANCE_LAYERS", None)
        names = ["agc_frametime", "agc_draw_frametime"]
    else:
        env["VK_INSTANCE_LAYERS"] = "VK_LAYER_KHRONOS_validation"
        env["VK_LOADER_DEBUG"] = "layer"
        names = ["agc_frametime", "agc_draw_frametime", "agc_submit_progress", "agc_submit_progress_no_timeline",
                 "agc_draw_descriptors", "agc_unwatched_revalidate", "agc_unwatched_revalidate_fast",
                 "agc_shader_table_snapshot_reuse", "agc_pipeline_cache", "agc_pipeline_library",
                 "agc_buffer_pool", "agc_resident_present", "agc_driver_presentation_pass",
                 "agc_sampled_storage_base_level_usage", "agc_multisample_guest_access",
                 "guest_shader_link", "guest_agc_driver_resources",
                 "agc_driver_depth_bounds_state", "agc_driver_unused_viewport_depth"]
    pattern = "^(" + "|".join(names) + ")$"
    if args.prepare:
        listing = subprocess.run([args.ctest, "--test-dir", str(args.build.resolve()), "-R", pattern,
                                  "--show-only=json-v1"], capture_output=True, text=True, check=True)
        tests = json.loads(listing.stdout)["tests"]
        missing = {test["name"] for test in tests if not test.get("command")}
        if missing:
            resolved = test_commands(args.build, missing)
            for test in tests:
                candidates = resolved.get(test["name"], set())
                if test["name"] in missing and len(candidates) == 1:
                    command = next(iter(candidates))
                    if command and os.path.isabs(command[0]):
                        test["command"] = list(command)
        targets = sorted({Path(test["command"][0]).stem for test in tests if test.get("command")})
        if not targets or any(re.fullmatch(r"[A-Za-z0-9_]+", target) is None for target in targets):
            raise RuntimeError("cannot identify graphics executable build targets")
        with (args.output / "build.log").open("w", encoding="utf-8") as log:
            built = subprocess.run([args.cmake, "--build", str(args.build.resolve()), "--target", *targets,
                                    "--parallel", "4"], env=env, stdout=log, stderr=subprocess.STDOUT)
        if built.returncode:
            print(json.dumps(dict(passed=False, build_exit=built.returncode, report=str(args.output / "build.log"))))
            return 1
    results = []
    for run in range(args.runs if args.performance else 1):
        command = [args.ctest, "--test-dir", str(args.build.resolve()), "-R", pattern,
                   "-V", "--output-junit", str((args.output / f"tests-{run}.xml").resolve())]
        started = time.monotonic()
        with (args.output / f"run-{run}.log").open("w", encoding="utf-8") as log:
            process = subprocess.run(command, env=env, stdout=log, stderr=subprocess.STDOUT)
        text = (args.output / f"run-{run}.log").read_text(encoding="utf-8", errors="replace")
        samples = [json.loads(value) for value in re.findall(r"AGC_BENCH (\{[^\n]+\})", text)]
        import xml.etree.ElementTree as ET
        xml = args.output / f"tests-{run}.xml"
        cases = list(ET.parse(xml).getroot().iter("testcase")) if xml.exists() else []
        actual = {case.attrib["name"] for case in cases}
        skipped = [case.attrib["name"] for case in cases if case.find("skipped") is not None]
        missing = sorted(set(names) - actual)
        validation_seen = "Insert instance layer" in text and "VK_LAYER_KHRONOS_validation" in text
        bad_validation = "VUID-" in text or "was not found but was requested" in text
        failed_budget = [sample for sample in samples if sample["stage"] == "total" and
                         (sample["p99_ms"] > args.budget_ms or sample["max_ms"] > args.max_ms)]
        valid = process.returncode == 0 and not missing and not skipped and len(samples) == 10 and not bad_validation
        if not args.performance:
            valid = valid and validation_seen
        results.append(dict(run=run, seconds=time.monotonic() - started, exit=process.returncode,
                            valid=valid, missing=missing, skipped=skipped, validation_seen=validation_seen,
                            vuid_count=text.count("VUID-"), measurements=samples, budget_failures=failed_budget))
    passed = all(result["valid"] and (not args.performance or not result["budget_failures"]) for result in results)
    repository = Path(__file__).resolve().parent.parent
    revision = subprocess.run(["git", "rev-parse", "HEAD"], cwd=repository, capture_output=True, text=True)
    dirty = subprocess.run(["git", "status", "--porcelain"], cwd=repository, capture_output=True, text=True)
    summary = dict(mode="performance" if args.performance else "correctness", budget_ms=args.budget_ms, max_ms=args.max_ms,
                   passed=passed, game_fps_verified=False, revision=revision.stdout.strip(), source_dirty=bool(dirty.stdout.strip()), runs=results)
    (args.output / "summary.json").write_text(json.dumps(summary, indent=2), encoding="utf-8")
    print(json.dumps(dict(passed=passed, report=str(args.output / "summary.json"), game_fps_verified=False)))
    return 0 if passed else 1


if __name__ == "__main__":
    sys.exit(main())
