import argparse
import collections
import hashlib
import json
import math
import os
from pathlib import Path
import re
import signal
import subprocess
import sys
import xml.etree.ElementTree as ET


FORCED = {"ANYPS5_REQUIRE_VULKAN": "1", "ANYPS5_NO_SHADER_CACHE": "1",
          "VK_INSTANCE_LAYERS": "VK_LAYER_KHRONOS_validation"}
PUBLIC_ENV = ("ANYPS5_GPU", "VK_ICD_FILENAMES", "VK_DRIVER_FILES", "VK_LAYER_PATH")
DIAGNOSTIC = re.compile(r"\b(VUID-[A-Za-z0-9_-]+|SYNC-HAZARD-[A-Za-z0-9_-]+)")
MESSAGE = re.compile(r"Validation (?:Error|Warning):|(?:VUID-|SYNC-HAZARD-)[\w-]+\s*\((?:ERROR|WARNING)\s*/")
MISSING_LAYER = "was not found but was requested by env var VK_INSTANCE_LAYERS"


class GateError(Exception):
    pass


def positive(value):
    number = float(value)
    if not math.isfinite(number) or number <= 0:
        raise argparse.ArgumentTypeError("must be finite and positive")
    return number


def git(source, *arguments):
    return subprocess.run(["git", "-C", str(source), *arguments], capture_output=True,
                          text=True, check=True).stdout.strip()


def identity(source, expected):
    if not re.fullmatch(r"[0-9a-f]{40}", expected):
        raise GateError("expected a full lowercase Git commit ID")
    if Path(git(source, "rev-parse", "--show-toplevel")).resolve() != source:
        raise GateError(f"{source}: must be a source worktree root")
    if git(source, "rev-parse", "HEAD") != expected:
        raise GateError(f"{source}: HEAD differs from {expected}")
    if git(source, "status", "--porcelain", "--untracked-files=normal"):
        raise GateError(f"{source}: source worktree is dirty")
    return {"source": str(source), "commit": expected}


def run(command, log, environment, timeout):
    with log.open("w", encoding="utf-8") as output:
        process = subprocess.Popen(command, stdin=subprocess.DEVNULL, stdout=output, stderr=subprocess.STDOUT,
                                   env=environment, start_new_session=True)
        try:
            return process.wait(timeout=timeout)
        finally:
            try:
                os.killpg(process.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
            process.wait()


def cache_values(build):
    result = {}
    for line in (build / "CMakeCache.txt").read_text(encoding="utf-8-sig").splitlines():
        match = re.match(r"([^#/:][^:]*):[A-Z]+=(.*)", line)
        if match:
            result[match[1]] = match[2]
    return result


def artifact_hashes(build, tests, runtime):
    paths = set()
    for test in tests:
        command = test.get("command") or []
        if not command or not Path(command[0]).is_file():
            raise GateError(f"{test['name']}: selected command is not an existing executable")
        working = next((Path(prop["value"]) for prop in test.get("properties", [])
                        if prop["name"] == "WORKING_DIRECTORY"), Path("."))
        if not working.is_absolute():
            working = build / working
        for argument in command:
            value = argument.partition("=")[2] if argument.startswith("-D") else argument
            path = Path(value)
            if not path.is_absolute():
                path = working / path
            if path.is_file():
                paths.add(path.resolve())
    for relative in runtime:
        path = (build / relative).resolve()
        if not path.is_relative_to(build) or not path.exists():
            raise GateError(f"runtime path must exist inside the build: {relative}")
        if path.is_dir():
            paths.update(p.resolve() for p in path.rglob("*") if p.is_file())
        else:
            paths.add(path)
    return {str(path): hashlib.sha256(path.read_bytes()).hexdigest() for path in sorted(paths)}


def check_timeouts(tests, limit):
    for test in tests:
        for prop in test.get("properties", []):
            if prop["name"] == "TIMEOUT_AFTER_MATCH":
                raise GateError(f"{test['name']}: TIMEOUT_AFTER_MATCH can restart the requested test timeout")
            if prop["name"] == "TIMEOUT":
                value = prop["value"]
                if type(value) not in (int, float) or not math.isfinite(value) or not 0 < value <= limit:
                    raise GateError(f"{test['name']}: TIMEOUT must be positive and at most {limit} seconds")


def parse_report(path, expected):
    root = ET.parse(path).getroot()
    if root.tag not in ("testsuite", "testsuites"):
        raise GateError(f"{path}: expected JUnit testsuite or testsuites")
    results, problems = {}, []
    for case in root.iter("testcase"):
        name = case.get("name")
        if not name or name in results:
            raise GateError(f"{path}: missing or duplicate testcase name")
        messages = collections.Counter()
        missing = []
        for channel in ("system-out", "system-err"):
            for line in (case.findtext(channel) or "").splitlines():
                if MESSAGE.search(line):
                    messages.update(DIAGNOSTIC.findall(line))
                if MISSING_LAYER in line:
                    missing.append(line)
        failed = (case.find("failure") is not None or case.find("error") is not None
                  or case.find("skipped") is not None or case.get("status") in ("disabled", "notrun"))
        if failed or missing:
            problems.append(name)
        results[name] = {"diagnostics": dict(messages), "missing_layers": missing, "failed_or_skipped": failed}
    if set(results) != set(expected):
        problems.append("test coverage differs: missing=" + repr(sorted(set(expected) - set(results)))
                        + " extra=" + repr(sorted(set(results) - set(expected))))
    return results, problems


def compare(iterations):
    counts = {name: collections.defaultdict(collections.Counter) for name in ("baseline", "candidate")}
    problems = []
    for iteration in iterations:
        if iteration["returncode"] or iteration["problems"]:
            problems.append({"side": iteration["side"], "iteration": iteration["iteration"],
                             "returncode": iteration["returncode"], "problems": iteration["problems"]})
        for test, result in iteration["tests"].items():
            counts[iteration["side"]][test].update(result["diagnostics"])
    new = {test: sorted(set(messages) - set(counts["baseline"][test]))
           for test, messages in counts["candidate"].items()
           if set(messages) - set(counts["baseline"][test])}
    return {"counts": {side: {test: dict(messages) for test, messages in tests.items()}
                       for side, tests in counts.items()}, "new_diagnostics": new, "problems": problems,
            "exit_code": int(bool(new or problems))}


def save(output, report):
    (output / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")


def main(argv=None):
    parser = argparse.ArgumentParser(description="Build and repeatedly compare focused Vulkan CTests on two pinned clean POSIX worktrees")
    parser.add_argument("--baseline", nargs=2, required=True, metavar=("SOURCE", "COMMIT"))
    parser.add_argument("--candidate", nargs=2, required=True, metavar=("SOURCE", "COMMIT"))
    parser.add_argument("--output", type=Path, required=True, help="new directory outside both source worktrees")
    parser.add_argument("--target", action="append", required=True, help="explicit CMake build target; repeat as needed")
    parser.add_argument("--tests", required=True, help="CTest regular expression; both builds must select the same nonempty test-name set")
    parser.add_argument("--gpu", required=True, help="expected ANYPS5_GPU selection; this is not independent hardware verification")
    parser.add_argument("--repeat", type=int, default=2)
    parser.add_argument("--timeout", type=positive, default=600, help="outer timeout per configure/build/CTest command")
    parser.add_argument("--test-timeout", type=positive, default=30)
    parser.add_argument("--cmake-arg", action="append", default=[], help="shared -D cache definition, passed as --cmake-arg=-DNAME=VALUE")
    parser.add_argument("--runtime", action="append", default=[], help="build-relative runtime file/directory to hash before and after tests")
    parser.add_argument("--cmake", default="cmake")
    parser.add_argument("--ctest", default="ctest")
    if os.name != "posix":
        parser.error("execution is currently supported only on POSIX; Windows process containment is not implemented")
    args = parser.parse_args(argv)
    if args.repeat <= 0:
        parser.error("--repeat must be positive")
    if args.test_timeout > 30:
        parser.error("--test-timeout must be at most 30 seconds")
    if any(not re.fullmatch(r"-D[A-Za-z_][A-Za-z0-9_]*(?::[A-Z]+)?=.*", value) for value in args.cmake_arg):
        parser.error("--cmake-arg accepts only -D cache definitions")
    report = {"status": "incomplete", "iterations": [], "builds": {},
              "limits": "GPU selection is requested, not independently verified. Only selected command files and explicit --runtime paths are hashed; other dynamic dependencies are not inventoried."}
    output = args.output.resolve()
    created = False
    try:
        sources = {side: (Path(values[0]).resolve(), values[1])
                   for side, values in (("baseline", args.baseline), ("candidate", args.candidate))}
        for source, expected in sources.values():
            identity(source, expected)
            if output.is_relative_to(source) or source.is_relative_to(output):
                raise GateError("output and source worktrees must be separate trees")
        output.mkdir(parents=True, exist_ok=False)
        created = True
        environment = {**os.environ, **FORCED, "ANYPS5_GPU": args.gpu}
        report["environment"] = {key: environment[key] for key in (*FORCED, *PUBLIC_ENV) if key in environment}
        report["options"] = {"targets": args.target, "tests": args.tests, "repeat": args.repeat,
                             "cmake_args": args.cmake_arg, "runtime": args.runtime}
        listings, hashes, contexts = {}, {}, {}
        for side, (source, expected) in sources.items():
            directory = output / side
            directory.mkdir()
            build = directory / "build"
            commands = [[args.cmake, "-S", str(source), "-B", str(build), "-G", "Ninja",
                         *args.cmake_arg, "-DCMAKE_BUILD_TYPE=Release", "-DBUILD_TESTING=ON"],
                        [args.cmake, "--build", str(build), "--target", *args.target]]
            record = report["builds"][side] = {**identity(source, expected), "commands": commands}
            save(output, report)
            for index, command in enumerate(commands):
                if run(command, directory / f"build-{index}.log", environment, args.timeout):
                    raise GateError(f"{side}: configure/build failed; see retained log")
            cache = cache_values(build)
            if Path(cache["CMAKE_HOME_DIRECTORY"]).resolve() != source:
                raise GateError(f"{side}: CMake source attribution differs")
            contexts[side] = {key: value for key, value in cache.items()
                              if key in ("CMAKE_GENERATOR", "CMAKE_BUILD_TYPE")
                              or re.fullmatch(r"CMAKE_(?:C|CXX)_COMPILER|CMAKE_(?:C|CXX|EXE_LINKER|SHARED_LINKER)_FLAGS(?:_RELEASE)?", key)}
            record["configuration"] = contexts[side]
            record["compiler_metadata"] = {}
            for path in (build / "CMakeFiles").glob("*/CMake*Compiler.cmake"):
                metadata = path.read_text(encoding="utf-8")
                record["compiler_metadata"][str(path.relative_to(build))] = re.findall(
                    r'set\((CMAKE_(?:C|CXX)_COMPILER_(?:ID|VERSION)) "([^"]*)"\)', metadata)
                contexts[side].update(record["compiler_metadata"][str(path.relative_to(build))])
            listing = directory / "tests.json"
            if run([args.ctest, "--test-dir", str(build), "-R", args.tests, "--show-only=json-v1"],
                   listing, environment, args.timeout):
                raise GateError(f"{side}: cannot list selected tests")
            tests = json.loads(listing.read_text(encoding="utf-8"))["tests"]
            names = [test["name"] for test in tests]
            if not names or len(names) != len(set(names)):
                raise GateError(f"{side}: expected nonempty unique test names")
            check_timeouts(tests, args.test_timeout)
            for test in tests:
                for prop in test.get("properties", []):
                    if prop["name"] in ("ENVIRONMENT", "ENVIRONMENT_MODIFICATION"):
                        for setting in prop["value"]:
                            key, _, value = setting.partition("=")
                            if key in (*FORCED, *PUBLIC_ENV) and (prop["name"] == "ENVIRONMENT_MODIFICATION" or value != environment.get(key)):
                                raise GateError(f"{test['name']}: test property overrides required environment {key}")
            listings[side] = tests
            record["tests"] = tests
            hashes[side] = artifact_hashes(build, tests, args.runtime)
            record["artifact_sha256"] = hashes[side]
            identity(source, expected)
        if contexts["baseline"] != contexts["candidate"]:
            raise GateError("compiler/build configuration differs between the two builds")
        names = {test["name"] for test in listings["baseline"]}
        if names != {test["name"] for test in listings["candidate"]}:
            raise GateError("selected test-name sets differ between builds")
        for iteration in range(1, args.repeat + 1):
            for side in (("baseline", "candidate") if iteration % 2 else ("candidate", "baseline")):
                directory = output / side
                junit = directory / f"iteration-{iteration}.xml"
                command = [args.ctest, "--test-dir", str(directory / "build"), "-R", args.tests, "-V", "--parallel", "1",
                           "--test-output-size-passed", "0", "--test-output-size-failed", "0",
                           "--timeout", str(args.test_timeout), "--output-junit", str(junit)]
                code = run(command, directory / f"iteration-{iteration}.log", environment, args.timeout)
                tests, problems = parse_report(junit, names)
                report["iterations"].append({"side": side, "iteration": iteration, "command": command,
                                              "returncode": code, "tests": tests, "problems": problems})
                save(output, report)
        for side, (source, expected) in sources.items():
            identity(source, expected)
            if artifact_hashes(output / side / "build", listings[side], args.runtime) != hashes[side]:
                raise GateError(f"{side}: selected command/runtime artifacts changed during testing")
        report.update(compare(report["iterations"]))
        report["status"] = "complete"
        save(output, report)
        print(f"{len(names)} tests, {args.repeat} iterations per build; report: {output / 'report.json'}")
        return report["exit_code"]
    except (GateError, OSError, ValueError, KeyError, ET.ParseError, subprocess.SubprocessError) as error:
        report["error"] = str(error)
        report["exit_code"] = 2
        if created:
            save(output, report)
        print(f"validation gate: {error}", file=sys.stderr)
        return 2
    except KeyboardInterrupt:
        report["exit_code"] = 130
        if created:
            save(output, report)
        return 130


if __name__ == "__main__":
    sys.exit(main())
