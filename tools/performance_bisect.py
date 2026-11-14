import argparse
import json
import math
import os
import re
import shutil
import statistics
import subprocess
import sys
import tempfile
from pathlib import Path


TOOLS_DIR = Path(__file__).resolve().parent
PLACEHOLDERS = ("worktree", "build_dir", "result_dir", "commit")
TITLE_ID = re.compile(r"[A-Z]{4}[0-9]{5}")
RECIPE_FIELDS = {"title_id", "build_commands", "build_dir", "games", "seconds", "repetitions",
                 "regression_pct", "max_spread_pct", "titles", "content"}


def load_recipe(path):
    recipe_path = Path(path).resolve()
    recipe = json.loads(recipe_path.read_text(encoding="utf-8"))
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
    if not isinstance(build_dir, str) or not build_dir or Path(build_dir).is_absolute() or ".." in Path(build_dir).parts or Path(build_dir) == Path("."):
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
    if type(regression_pct) not in (int, float) or not math.isfinite(regression_pct) or not 0 < regression_pct < 100:
        raise ValueError("recipe.regression_pct must be between 0 and 100")
    max_spread_pct = recipe.get("max_spread_pct", 3)
    if type(max_spread_pct) not in (int, float) or not math.isfinite(max_spread_pct) or not 0 < max_spread_pct < 100:
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
    title = summary.get("titles", {}).get(title_id)
    if not isinstance(machine, dict) or not isinstance(title, dict):
        raise ValueError(f"{path}: title runner summary is missing machine or title data")
    run_list = title.get("runs")
    if not isinstance(run_list, list) or len(run_list) != 1 or not isinstance(run_list[0], dict):
        raise ValueError(f"{path}: expected exactly one measured title run")
    run = run_list[0]
    fps = run.get("fps")
    if run.get("outcome") != "rendering" or type(fps) not in (int, float) or not math.isfinite(fps) or fps <= 0:
        raise ValueError(f"{path}: title did not render with a finite positive FPS measurement")
    identity = {key: machine.get(key, "") for key in ("os", "gpu", "driver", "cpu")}
    identity.update(title_id=title_id, title=title.get("name", ""), content_version=title.get("version", ""))
    if any(not identity[key] for key in ("os", "gpu", "driver", "title")):
        raise ValueError(f"{path}: measurement is missing machine, title, or content identity")
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
    return fps <= baseline_fps * (1 - threshold_pct / 100)


def require_same_identity(baseline, measured):
    if baseline["identity"] != measured["identity"]:
        raise ValueError("machine, title, or content identity changed during the bisect")


def git(repo, *args):
    result = subprocess.run(["git", "-C", str(repo), *args], capture_output=True, text=True, encoding="utf-8", errors="replace")
    if result.returncode:
        raise RuntimeError(result.stderr.strip() or f"git {' '.join(args)} failed")
    return result.stdout.strip()


def first_parent_commits(repo, good, bad):
    good_sha = git(repo, "rev-parse", "--verify", f"{good}^{{commit}}")
    bad_sha = git(repo, "rev-parse", "--verify", f"{bad}^{{commit}}")
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
    result = []
    for argument in command:
        for name in PLACEHOLDERS:
            argument = argument.replace("{" + name + "}", str(values[name]))
        result.append(argument)
    return result


def reset_build_dir(worktree, build_dir):
    root = worktree.resolve()
    if not build_dir.resolve().is_relative_to(root):
        raise ValueError("recipe.build_dir resolves outside the worktree")
    relative = build_dir.relative_to(worktree).as_posix()
    if git(worktree, "ls-files", "--", relative):
        raise ValueError("recipe.build_dir contains tracked source files and cannot be cleared safely")
    if build_dir.is_symlink():
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


def measure_commit(repo, worktree, results_root, recipe, commit):
    git(worktree, "checkout", "--detach", "--force", commit)
    commit_results = results_root / commit
    commit_results.mkdir()
    build_dir = run_build_commands(worktree, recipe, commit, commit_results)
    samples = []
    for sample_number in range(1, recipe["repetitions"] + 1):
        sample = run_sample(worktree, build_dir, commit_results / f"sample-{sample_number}", recipe, commit)
        samples.append(sample)
        print(f"{commit[:12]} sample {sample_number}: {sample['fps']:.2f} FPS", file=sys.stderr, flush=True)
    result = summarize(samples, recipe["max_spread_pct"])
    print(f"{commit[:12]} median: {result['median_fps']:.2f} FPS; spread {result['spread_pct']:.2f}%", file=sys.stderr, flush=True)
    return result


def write_report(path, report):
    output = Path(path)
    if output.exists():
        raise ValueError(f"report already exists: {output}")
    output.parent.mkdir(parents=True, exist_ok=True)
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(mode="w", encoding="utf-8", dir=output.parent,
                                         prefix=output.name + ".", suffix=".tmp", delete=False) as file:
            temporary = Path(file.name)
            file.write(json.dumps(report, indent=2, ensure_ascii=False, allow_nan=False) + "\n")
        os.link(temporary, output)
    finally:
        if temporary is not None:
            temporary.unlink(missing_ok=True)



def validate_regression_endpoints(baseline, endpoint, threshold_pct):
    require_same_identity(baseline, endpoint)
    if not is_regression(endpoint["median_fps"], baseline["median_fps"], threshold_pct):
        raise ValueError(f"bad endpoint is not at least {threshold_pct:.2f}% slower than the good endpoint")



def find_first_bad_commit(repo, worktree, results, recipe, good_sha, baseline, bad_sha, endpoint, commits):
    low, high = 0, len(commits)
    measurements = {good_sha: baseline, bad_sha: endpoint}
    while high - low > 1:
        middle = (low + high) // 2
        commit = commits[middle - 1]
        measured = measure_commit(repo, worktree, results, recipe, commit)
        require_same_identity(baseline, measured)
        measurements[commit] = measured
        if is_regression(measured["median_fps"], baseline["median_fps"], recipe["regression_pct"]):
            high = middle
        else:
            low = middle
    first_bad = commits[high - 1]
    previous_good = good_sha if high == 1 else commits[high - 2]
    if previous_good not in measurements:
        measurements[previous_good] = measure_commit(repo, worktree, results, recipe, previous_good)
        require_same_identity(baseline, measurements[previous_good])
    return first_bad, previous_good, measurements


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


def bisect(repo, good, bad, recipe, output_dir, temp_dir=None):
    repo = Path(repo).resolve()
    if output_dir is not None and Path(output_dir).exists():
        raise ValueError(f"report already exists: {output_dir}")
    good_sha, bad_sha, commits = first_parent_commits(repo, good, bad)
    with tempfile.TemporaryDirectory(prefix="anyps5-performance-bisect-", dir=temp_dir) as temporary:
        root = Path(temporary)
        worktree = root / "source"
        results = root / "results"
        git(repo, "worktree", "add", "--detach", str(worktree), good_sha)
        try:
            baseline = measure_commit(repo, worktree, results, recipe, good_sha)
            endpoint = measure_commit(repo, worktree, results, recipe, bad_sha)
            validate_regression_endpoints(baseline, endpoint, recipe["regression_pct"])
            first_bad, previous_good, measurements = find_first_bad_commit(
                repo, worktree, results, recipe, good_sha, baseline, bad_sha, endpoint, commits)
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
    parser.add_argument("--temp-dir", type=Path, help="existing directory with enough space for a build worktree")
    return parser


def main(argv=None):
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, "reconfigure"):
            stream.reconfigure(encoding="utf-8", errors="backslashreplace")
    parser = build_parser()
    args = parser.parse_args(argv)
    try:
        if args.temp_dir is not None and not args.temp_dir.is_dir():
            raise ValueError("--temp-dir must name an existing directory")
        recipe = load_recipe(args.recipe)
        report = bisect(args.repo, args.good, args.bad, recipe, args.output, args.temp_dir)
    except (OSError, ValueError, RuntimeError, subprocess.SubprocessError, json.JSONDecodeError) as error:
        parser.exit(2, f"{parser.prog}: {error}\n")
    print(f"first measured regression: {report['first_bad']['commit']} ({report['first_bad']['median_fps']:.2f} FPS)")
    print(f"previous measured good commit: {report['previous_good']['commit']} ({report['previous_good']['median_fps']:.2f} FPS)")
    if args.output:
        print(f"report written to {args.output}")
    return 0



if __name__ == "__main__":
    raise SystemExit(main())
