import argparse
from contextlib import contextmanager, nullcontext
import errno
import hashlib
import json
import math
import os
import re
import shutil
import statistics
import subprocess
import sys
import tempfile
from fractions import Fraction
from pathlib import Path


TOOLS_DIR = Path(__file__).resolve().parent
PLACEHOLDER = re.compile(r"\{(worktree|build_dir|result_dir|commit)\}")
TITLE_ID = re.compile(r"[A-Z]{4}[0-9]{5}")
RECIPE_FIELDS = {"title_id", "build_commands", "build_dir", "games", "seconds", "repetitions",
                 "regression_pct", "max_spread_pct", "titles", "content"}
CHECKPOINT_FORMAT = "anyps5-performance-bisect-checkpoint"
SAMPLE_IDENTITY_FIELDS = {"os", "gpu", "driver", "cpu", "title_id", "title", "content_version"}


def unique_recipe_fields(pairs):
    fields = {}
    for name, value in pairs:
        if name in fields:
            raise ValueError(f"recipe has a duplicate field: {name}")
        fields[name] = value
    return fields


def load_recipe(path):
    recipe_path = Path(path).resolve()
    recipe = json.loads(recipe_path.read_text(encoding="utf-8-sig"), object_pairs_hook=unique_recipe_fields)
    if not isinstance(recipe, dict):
        raise ValueError("recipe must be a JSON object")
    unknown = set(recipe) - RECIPE_FIELDS
    if unknown:
        raise ValueError(f"recipe has unknown fields: {', '.join(sorted(unknown))}")
    title_id = recipe.get("title_id")
    if not isinstance(title_id, str) or not TITLE_ID.fullmatch(title_id):
        raise ValueError("recipe.title_id must be a title ID such as PPSA00000")
    commands = recipe.get("build_commands")
    if not isinstance(commands, list) or not commands:
        raise ValueError("recipe.build_commands must contain at least one command")
    for number, command in enumerate(commands, 1):
        if not isinstance(command, list) or not command or any(not isinstance(item, str) or not item for item in command):
            raise ValueError(f"recipe.build_commands[{number - 1}] must be a non-empty argument list")
    build_dir = recipe.get("build_dir", "build")
    if not isinstance(build_dir, str) or not build_dir or Path(build_dir).anchor or ".." in Path(build_dir).parts or Path(build_dir) == Path("."):
        raise ValueError("recipe.build_dir must be a non-root relative path inside the worktree")
    games = recipe.get("games")
    if not isinstance(games, list) or not games:
        raise ValueError("recipe.games must list existing extracted-game directories")
    normalized_games = []
    for game in games:
        if not isinstance(game, str) or not game:
            raise ValueError("recipe.games entries must be non-empty paths")
        game_path = Path(game)
        if not game_path.is_absolute():
            game_path = recipe_path.parent / game_path
        if not game_path.is_dir():
            raise ValueError(f"recipe.games entry is not an existing directory: {game}")
        normalized_games.append(str(game_path.resolve()))
    seconds = recipe.get("seconds", 60)
    if type(seconds) is not int or seconds < 20:
        raise ValueError("recipe.seconds must be an integer of at least 20")
    repetitions = recipe.get("repetitions", 3)
    if type(repetitions) is not int or repetitions < 3:
        raise ValueError("recipe.repetitions must be an integer of at least 3")
    regression_pct = recipe.get("regression_pct", 5)
    if type(regression_pct) not in (int, float) or not 0 < regression_pct < 100:
        raise ValueError("recipe.regression_pct must be between 0 and 100")
    max_spread_pct = recipe.get("max_spread_pct", 3)
    if type(max_spread_pct) not in (int, float) or not 0 < max_spread_pct < 100:
        raise ValueError("recipe.max_spread_pct must be between 0 and 100")
    titles = recipe.get("titles", str(TOOLS_DIR / "titles.json"))
    if not isinstance(titles, str):
        raise ValueError("recipe.titles must be a path string")
    titles_path = Path(titles)
    if not titles_path.is_absolute():
        titles_path = recipe_path.parent / titles_path
    if not titles_path.is_file():
        raise ValueError("recipe.titles must name an existing title catalog")
    content = recipe.get("content")
    content_path = None
    if content is not None:
        if not isinstance(content, str) or not content:
            raise ValueError("recipe.content must name an existing content directory")
        content_path = Path(content)
        if not content_path.is_absolute():
            content_path = recipe_path.parent / content_path
        if not content_path.is_dir():
            raise ValueError("recipe.content must name an existing content directory")
    recipe.update(build_dir=build_dir, games=normalized_games, seconds=seconds, repetitions=repetitions,
                  regression_pct=float(regression_pct), max_spread_pct=float(max_spread_pct),
                  titles=str(titles_path.resolve()), content=str(content_path.resolve()) if content_path is not None else None)
    return recipe


def sample_summary(path, title_id, commit):
    summary = json.loads(Path(path).read_text(encoding="utf-8"))
    if not isinstance(summary, dict) or not isinstance(summary.get("build"), dict):
        raise ValueError(f"{path}: title runner summary has no build identity")
    build = summary["build"]
    built_commit = build.get("commit")
    if not isinstance(built_commit, str) or not 7 <= len(built_commit) <= 40 or not commit.startswith(built_commit) or build.get("dirty") is not False:
        raise ValueError(f"{path}: measured build is not the requested clean commit {commit}")
    machine = summary.get("machine")
    titles = summary.get("titles")
    title = titles.get(title_id) if isinstance(titles, dict) else None
    if not isinstance(machine, dict) or not isinstance(title, dict):
        raise ValueError(f"{path}: title runner summary is missing machine or title data")
    run_list = title.get("runs")
    if not isinstance(run_list, list) or len(run_list) != 1 or not isinstance(run_list[0], dict):
        raise ValueError(f"{path}: expected exactly one measured title run")
    run = run_list[0]
    fps = run.get("fps")
    if run.get("outcome") != "rendering" or type(fps) not in (int, float) or not 0 < fps <= sys.float_info.max:
        raise ValueError(f"{path}: title did not render with a finite positive FPS measurement")
    identity = {key: machine.get(key, "") for key in ("os", "gpu", "driver", "cpu")}
    identity.update(title_id=title_id, title=title.get("name", ""), content_version=title.get("version", ""))
    invalid = [key for key in ("os", "gpu", "driver", "cpu", "title")
               if not isinstance(identity[key], str) or not identity[key].strip()]
    if invalid:
        raise ValueError(f"{path}: measurement is missing machine or title identity text for {', '.join(invalid)}")
    content_version = identity["content_version"]
    if not isinstance(content_version, str) or not content_version.strip():
        raise ValueError(f"{path}: measurement is missing a valid content version")
    return {"commit": commit, "fps": float(fps), "identity": identity, "outcome": run["outcome"]}


def summarize(samples, max_spread_pct):
    values = [sample["fps"] for sample in samples]
    if not values:
        raise ValueError("no FPS samples were collected")
    median = statistics.median(values)
    spread_pct = (max(values) - min(values)) / median * 100 if median else math.inf
    if spread_pct > max_spread_pct:
        raise ValueError(f"measurement spread {spread_pct:.2f}% exceeds the {max_spread_pct:.2f}% limit")
    identity = samples[0]["identity"]
    if any(sample["identity"] != identity for sample in samples[1:]):
        raise ValueError("machine, title, or content identity changed between repeated measurements")
    return {"median_fps": median, "spread_pct": spread_pct, "samples": samples, "identity": identity}


def is_regression(fps, baseline_fps, threshold_pct):
    baseline = Fraction(str(baseline_fps))
    return (baseline - Fraction(str(fps))) * 100 >= baseline * Fraction(str(threshold_pct))


def require_same_identity(baseline, measured):
    if baseline["identity"] != measured["identity"]:
        raise ValueError("machine, title, or content identity changed during the bisect")


def git(repo, *args):
    result = subprocess.run(["git", "-C", str(repo), *args], capture_output=True, text=True, encoding="utf-8", errors="replace")
    if result.returncode:
        raise RuntimeError(result.stderr.strip() or f"git {' '.join(args)} failed")
    return result.stdout.strip()


def first_parent_commits(repo, good, bad):
    good_sha = git(repo, "rev-parse", "--verify", f"{good}^0")
    bad_sha = git(repo, "rev-parse", "--verify", f"{bad}^0")
    result = subprocess.run(["git", "-C", str(repo), "merge-base", "--is-ancestor", good_sha, bad_sha], capture_output=True)
    if result.returncode:
        raise ValueError("good commit must be an ancestor of bad commit")
    first_parent_history = git(repo, "rev-list", "--first-parent", bad_sha).splitlines()
    if good_sha not in first_parent_history:
        raise ValueError("good commit must be an ancestor of bad commit on the first-parent history")
    commits = git(repo, "rev-list", "--first-parent", "--reverse", f"{good_sha}..{bad_sha}").splitlines()
    if not commits or commits[-1] != bad_sha:
        raise ValueError("the bad commit must follow the good commit on the first-parent history")
    return good_sha, bad_sha, commits


def format_command(command, values):
    return [PLACEHOLDER.sub(lambda match: str(values[match[1]]), argument) for argument in command]


def reset_build_dir(worktree, build_dir):
    root = worktree.resolve()
    resolved = build_dir.resolve()
    if not resolved.is_relative_to(root):
        raise ValueError("recipe.build_dir resolves outside the worktree")
    relative = build_dir.relative_to(worktree).as_posix()
    is_link = build_dir.is_symlink()
    paths = [relative]
    resolved_relative = resolved.relative_to(root).as_posix()
    if not is_link and resolved_relative != relative:
        paths.append(resolved_relative)
    if git(worktree, "--literal-pathspecs", "ls-files", "--", *paths):
        raise ValueError("recipe.build_dir contains tracked source files and cannot be cleared safely")
    if is_link:
        build_dir.unlink()
    elif build_dir.exists():
        shutil.rmtree(build_dir)


def run_build_commands(worktree, recipe, commit, results_dir):
    build_dir = worktree / recipe["build_dir"]
    reset_build_dir(worktree, build_dir)
    values = {"worktree": worktree, "build_dir": build_dir, "result_dir": results_dir, "commit": commit}
    for command in recipe["build_commands"]:
        expanded = format_command(command, values)
        result = subprocess.run(expanded, cwd=worktree, capture_output=True, text=True, encoding="utf-8", errors="replace")
        if result.returncode:
            details = (result.stdout + result.stderr)[-8000:]
            raise RuntimeError(f"build command failed ({result.returncode}): {expanded!r}\n{details}")
    return build_dir


def run_sample(worktree, build_dir, result_dir, recipe, commit):
    result_dir.mkdir(parents=True)
    command = [sys.executable, str(TOOLS_DIR / "title_run.py"), recipe["title_id"]]
    for game_dir in recipe["games"]:
        command.extend(("--games", game_dir))
    command.extend(("--build", str(build_dir), "--out", str(result_dir), "--runs", "1", "--seconds", str(recipe["seconds"]), "--titles", recipe["titles"]))
    if recipe["content"] is not None:
        command.extend(("--content", recipe["content"]))
    result = subprocess.run(command, cwd=worktree, capture_output=True, text=True, encoding="utf-8", errors="replace")
    if result.returncode:
        raise RuntimeError(f"title run failed for {commit}: {(result.stdout + result.stderr)[-8000:]}")
    summaries = list(result_dir.glob("*/summary.json"))
    if len(summaries) != 1:
        raise RuntimeError(f"title run for {commit} produced {len(summaries)} summary files; expected exactly one")
    return sample_summary(summaries[0], recipe["title_id"], commit)


def measure_commit(repo, worktree, results_root, recipe, commit, *, repetitions=None):
    git(worktree, "checkout", "--detach", "--force", commit)
    commit_results = results_root / commit
    commit_results.mkdir()
    build_dir = run_build_commands(worktree, recipe, commit, commit_results)
    samples = []
    sample_count = recipe["repetitions"] if repetitions is None else repetitions
    for sample_number in range(1, sample_count + 1):
        sample = run_sample(worktree, build_dir, commit_results / f"sample-{sample_number}", recipe, commit)
        samples.append(sample)
        print(f"{commit[:12]} sample {sample_number}: {sample['fps']:.2f} FPS", file=sys.stderr, flush=True)
    result = summarize(samples, recipe["max_spread_pct"])
    print(f"{commit[:12]} median: {result['median_fps']:.2f} FPS; spread {result['spread_pct']:.2f}%", file=sys.stderr, flush=True)
    return result


def _hardlinks_unavailable(error):
    if isinstance(error, NotImplementedError):
        return True
    unsupported = {
        getattr(errno, name, None)
        for name in ("EPERM", "EACCES", "ENOTSUP", "EOPNOTSUPP", "EXDEV", "EMLINK")
    } - {None}
    return error.errno in unsupported or getattr(error, "winerror", None) in (1, 5)


@contextmanager
def prepared_json(output, report):
    output.parent.mkdir(parents=True, exist_ok=True)
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(mode="w", encoding="utf-8", dir=output.parent,
                                         prefix=output.name + ".", suffix=".tmp", delete=False) as file:
            temporary = Path(file.name)
            file.write(json.dumps(report, indent=2, ensure_ascii=False, allow_nan=False) + "\n")
        yield temporary
    finally:
        if temporary is not None:
            temporary.unlink(missing_ok=True)


def write_report(path, report):
    output = Path(path)
    if output.exists():
        raise ValueError(f"report already exists: {output}")
    with prepared_json(output, report) as temporary:
        try:
            os.link(temporary, output)
        except (OSError, NotImplementedError) as error:
            if not _hardlinks_unavailable(error):
                raise
            if os.name == "nt":
                os.rename(temporary, output)
            else:
                if output.exists():
                    raise ValueError(f"report already exists: {output}") from error
                os.replace(temporary, output)


@contextmanager
def checkpoint_lock(path):
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.with_name(path.name + ".lock").open("a+b") as lock:
        lock.seek(0)
        try:
            if os.name == "nt":
                import msvcrt
                msvcrt.locking(lock.fileno(), msvcrt.LK_NBLCK, 1)
            else:
                import fcntl
                fcntl.flock(lock.fileno(), fcntl.LOCK_EX | fcntl.LOCK_NB)
        except OSError as error:
            raise ValueError(f"cannot acquire checkpoint lock: {path}: {error}") from error
        yield


def json_digest(value):
    encoded = json.dumps(value, sort_keys=True, allow_nan=False, separators=(",", ":")).encode("utf-8")
    return hashlib.sha256(encoded).hexdigest()


def checkpoint_context(good, bad, commits, recipe):
    return {
        "format": CHECKPOINT_FORMAT,
        "version": 1,
        "good": good,
        "bad": bad,
        "history_sha256": json_digest(commits),
        "recipe_sha256": json_digest(recipe),
        "titles_sha256": hashlib.sha256(Path(recipe["titles"]).read_bytes()).hexdigest(),
        "runner_sha256": hashlib.sha256((TOOLS_DIR / "title_run.py").read_bytes()).hexdigest(),
        "bisector_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
    }


def valid_checkpoint_sample(sample, commit, title_id):
    if not isinstance(sample, dict) or sample.get("commit") != commit or sample.get("outcome") != "rendering":
        return False
    fps = sample.get("fps")
    identity = sample.get("identity")
    return (type(fps) in (int, float) and 0 < fps <= sys.float_info.max
            and isinstance(identity, dict) and set(identity) == SAMPLE_IDENTITY_FIELDS
            and all(isinstance(value, str) and value.strip() for value in identity.values())
            and identity["title_id"] == title_id)


def load_checkpoint(path, context, recipe, commits):
    try:
        saved = json.loads(path.read_text(encoding="utf-8"), object_pairs_hook=unique_recipe_fields)
    except (UnicodeError, ValueError, RecursionError) as error:
        raise ValueError(f"invalid checkpoint JSON: {path}") from error
    if (not isinstance(saved, dict) or saved.get("format") != CHECKPOINT_FORMAT
            or type(saved.get("version")) is not int or saved["version"] != context["version"]):
        raise ValueError("expected a version 1 AnyPS5 performance bisect checkpoint")
    if any(saved.get(key) != context[key] for key in ("good", "bad", "history_sha256")):
        raise ValueError("checkpoint endpoints or first-parent history changed")
    if any(saved.get(key) != context[key] for key in ("recipe_sha256", "titles_sha256", "runner_sha256", "bisector_sha256")):
        raise ValueError("checkpoint recipe or runner changed")
    rows = saved.get("measurements")
    if not isinstance(rows, dict) or (rows and context["good"] not in rows):
        raise ValueError("checkpoint measurements need the original good endpoint")
    allowed = {context["good"], *commits}
    result = {}
    for commit, samples in rows.items():
        if (commit not in allowed or not isinstance(samples, list) or len(samples) != recipe["repetitions"]
                or not all(valid_checkpoint_sample(sample, commit, recipe["title_id"]) for sample in samples)):
            raise ValueError(f"invalid completed checkpoint measurement for {commit!r}")
        result[commit] = summarize(samples, recipe["max_spread_pct"])
    if result:
        for measurement in result.values():
            require_same_identity(result[context["good"]], measurement)
    return result


def save_checkpoint(path, context, measurements):
    report = {**context, "measurements": {commit: row["samples"] for commit, row in measurements.items()}}
    with prepared_json(path, report) as temporary:
        os.replace(temporary, path)



def validate_regression_endpoints(baseline, endpoint, threshold_pct):
    require_same_identity(baseline, endpoint)
    if not is_regression(endpoint["median_fps"], baseline["median_fps"], threshold_pct):
        raise ValueError(f"bad endpoint is not at least {threshold_pct:.2f}% slower than the good endpoint")



def find_first_bad_commit(good_sha, baseline_fps, commits, threshold_pct, measure):
    low, high = 0, len(commits)
    while high - low > 1:
        middle = (low + high) // 2
        commit = commits[middle - 1]
        measured = measure(commit)
        if is_regression(measured["median_fps"], baseline_fps, threshold_pct):
            high = middle
        else:
            low = middle
    first_bad = commits[high - 1]
    previous_good = good_sha if high == 1 else commits[high - 2]
    measure(previous_good)
    return first_bad, previous_good


def make_report(good_sha, bad_sha, commits, first_bad, previous_good, measurements, recipe):
    return {
        "method": "binary search over first-parent commits",
        "assumption": "the regression is monotonic between the supplied good and bad commits",
        "metric": "median FPS across repeated title runs; lower is slower",
        "regression_pct": recipe["regression_pct"],
        "max_spread_pct": recipe["max_spread_pct"],
        "good": {"commit": good_sha, **measurements[good_sha]},
        "first_bad": {"commit": first_bad, **measurements[first_bad]},
        "previous_good": {"commit": previous_good, **measurements[previous_good]},
        "bad": {"commit": bad_sha, **measurements[bad_sha]},
        "commits_between": len(commits),
        "tested_commits": len(measurements),
        "measurements": [{"commit": commit, **measurements[commit]}
                         for commit in commits if commit in measurements],
    }


def bisect(repo, good, bad, recipe, output_dir, temp_dir=None, *, checkpoint=None, resume=False):
    repo = Path(repo).resolve()
    if output_dir is not None and Path(output_dir).exists():
        raise ValueError(f"report already exists: {output_dir}")
    if resume and checkpoint is None:
        raise ValueError("--resume requires --checkpoint")
    path = Path(checkpoint).resolve() if checkpoint is not None else None
    if path is not None and output_dir is not None:
        if Path(output_dir).resolve() in (path, path.with_name(path.name + ".lock")):
            raise ValueError("--output must differ from the checkpoint and its lock file")
    if resume and not path.is_file():
        raise ValueError(f"checkpoint does not exist: {path}")
    good_sha, bad_sha, commits = first_parent_commits(repo, good, bad)
    guard = checkpoint_lock(path) if path is not None else nullcontext()
    with guard, tempfile.TemporaryDirectory(prefix="anyps5-performance-bisect-", dir=temp_dir) as temporary:
        context, measurements = None, {}
        if path is not None:
            context = checkpoint_context(good_sha, bad_sha, commits, recipe)
            if resume:
                measurements = load_checkpoint(path, context, recipe, commits)
            else:
                if path.exists():
                    raise ValueError(f"checkpoint already exists: {path}. Use --resume to continue")
                write_report(path, {**context, "measurements": {}})
        root = Path(temporary).resolve()
        worktree = root / "source"
        results = root / "results"
        results.mkdir()
        git(repo, "worktree", "add", "--detach", str(worktree), good_sha)
        try:
            if measurements:
                witness = measure_commit(repo, worktree, results, recipe, good_sha, repetitions=1)
                baseline_samples = measurements[good_sha]["samples"]
                try:
                    summarize([*baseline_samples, *witness["samples"]], recipe["max_spread_pct"])
                except ValueError as error:
                    raise ValueError(f"cannot resume checkpoint: {error}") from error

            def measure(commit):
                if commit in measurements:
                    return measurements[commit]
                measured = measure_commit(repo, worktree, results, recipe, commit)
                if measurements:
                    require_same_identity(measurements[good_sha], measured)
                measurements[commit] = measured
                if path is not None:
                    save_checkpoint(path, context, measurements)
                return measured

            baseline = measure(good_sha)
            endpoint = measure(bad_sha)
            validate_regression_endpoints(baseline, endpoint, recipe["regression_pct"])
            first_bad, previous_good = find_first_bad_commit(
                good_sha, baseline["median_fps"], commits, recipe["regression_pct"], measure)
            report = make_report(good_sha, bad_sha, commits, first_bad, previous_good, measurements, recipe)
            if output_dir is not None:
                write_report(output_dir, report)
            return report
        finally:
            git(repo, "worktree", "remove", "--force", str(worktree))


def build_parser():
    parser = argparse.ArgumentParser(
        description="Find the first first-parent commit with a repeatable title FPS regression.",
        epilog=("Recipe JSON fields: title_id, games (extracted game directories), and build_commands "
                "(argument arrays; placeholders are {worktree}, {build_dir}, {result_dir}, {commit}). "
                "Optional fields: build_dir, seconds (>=20), repetitions (>=3), regression_pct, "
                "max_spread_pct, titles, and content. Commands run without a shell in an isolated worktree. "
                "The same title, machine identity, and content version are required throughout; "
                "high sample spread aborts instead of guessing. The result assumes one monotonic regression "
                "on the first-parent history."),
        formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--repo", type=Path, default=Path.cwd(), help="Git repository to bisect")
    parser.add_argument("--good", required=True, help="known-good commit")
    parser.add_argument("--bad", required=True, help="known-bad commit")
    parser.add_argument("--recipe", required=True, type=Path, help="JSON build and title-run recipe")
    parser.add_argument("--output", type=Path, help="write the measured bracket as JSON")
    parser.add_argument("--checkpoint", type=Path, help="save completed measurements to this file")
    parser.add_argument("--resume", action="store_true", help="resume an existing --checkpoint")
    parser.add_argument("--temp-dir", type=Path, help="existing directory with enough space for a build worktree")
    return parser


def main(argv=None):
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, "reconfigure"):
            stream.reconfigure(encoding="utf-8", errors="backslashreplace")
    parser = build_parser()
    args = parser.parse_args(argv)
    if args.resume and args.checkpoint is None:
        parser.error("--resume requires --checkpoint")
    try:
        if args.temp_dir is not None and not args.temp_dir.is_dir():
            raise ValueError("--temp-dir must name an existing directory")
        recipe = load_recipe(args.recipe)
        report = bisect(args.repo, args.good, args.bad, recipe, args.output, args.temp_dir,
                        checkpoint=args.checkpoint, resume=args.resume)
    except (OSError, ValueError, RuntimeError, subprocess.SubprocessError, json.JSONDecodeError) as error:
        parser.exit(2, f"{parser.prog}: {error}\n")
    print(f"first measured regression: {report['first_bad']['commit']} ({report['first_bad']['median_fps']:.2f} FPS)")
    print(f"previous measured good commit: {report['previous_good']['commit']} ({report['previous_good']['median_fps']:.2f} FPS)")
    if args.output:
        print(f"report written to {args.output}")
    return 0



if __name__ == "__main__":
    raise SystemExit(main())
