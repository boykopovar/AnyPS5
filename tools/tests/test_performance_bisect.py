from pathlib import Path
import json
import math
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import performance_bisect


GOOD = "1" * 40
IDENTITY = {"os": "Linux", "gpu": "GPU", "driver": "Driver", "cpu": "CPU",
            "title_id": "PPSA00000", "title": "Example", "content_version": "01.00"}


class RecipeTests(unittest.TestCase):
    def make_recipe(self, root):
        game = root / "game"
        game.mkdir()
        titles = root / "titles.json"
        titles.write_text("{}", encoding="utf-8")
        return {"title_id": "PPSA00000", "build_commands": [["cmake", "--build", "{build_dir}"]],
                "build_dir": "build", "games": [str(game)], "titles": str(titles), "seconds": 20,
                "repetitions": 3, "regression_pct": 5, "max_spread_pct": 3}

    def test_loads_valid_recipe_and_normalizes_external_paths(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            recipe_path = root / "recipe.json"
            recipe_path.write_text(json.dumps(self.make_recipe(root)), encoding="utf-8")
            recipe = performance_bisect.load_recipe(recipe_path)
            self.assertEqual(recipe["title_id"], "PPSA00000")
            self.assertEqual(recipe["build_dir"], "build")
            self.assertEqual(recipe["repetitions"], 3)
            self.assertEqual(recipe["titles"], str((root / "titles.json").resolve()))
            self.assertEqual(recipe["games"], [str((root / "game").resolve())])

    def test_rejects_recipe_typo_and_bad_title_id(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            recipe = self.make_recipe(root)
            for key, value in (("title_id", "not-a-title"), ("build_commands", []),
                               ("build_dir", "../outside"), ("build_dir", "."), ("seconds", 10),
                               ("repetitions", 2), ("regression_pct", 100),
                               ("max_spread_pct", math.inf)):
                with self.subTest(key=key):
                    bad = dict(recipe, **{key: value})
                    path = root / f"{key}.json"
                    path.write_text(json.dumps(bad), encoding="utf-8")
                    with self.assertRaises(ValueError):
                        performance_bisect.load_recipe(path)

    def test_resolves_relative_paths_from_the_recipe_file(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            config = root / "config"
            config.mkdir()
            game = config / "packages"
            game.mkdir()
            (config / "titles.json").write_text("{}", encoding="utf-8")
            recipe = self.make_recipe(root)
            recipe.update(games=["packages"], titles="titles.json")
            path = config / "recipe.json"
            path.write_text(json.dumps(recipe), encoding="utf-8")
            loaded = performance_bisect.load_recipe(path)
            self.assertEqual(loaded["games"], [str(game.resolve())])
            self.assertEqual(loaded["titles"], str((config / "titles.json").resolve()))

    def test_rejects_empty_game_paths(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            recipe = self.make_recipe(root)
            recipe["games"] = [""]
            path = root / "recipe.json"
            path.write_text(json.dumps(recipe), encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "non-empty paths"):
                performance_bisect.load_recipe(path)

    def test_rejects_unknown_fields_and_non_string_optional_paths(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            recipe = self.make_recipe(root)
            for key, value in (("repetiton", 3), ("titles", []), ("content", 4)):
                with self.subTest(key=key):
                    path = root / f"invalid-{key}.json"
                    path.write_text(json.dumps(dict(recipe, **{key: value})), encoding="utf-8")
                    with self.assertRaises(ValueError):
                        performance_bisect.load_recipe(path)


class SummaryValidationTests(unittest.TestCase):
    def test_sample_requires_clean_expected_commit_and_rendering_result(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "summary.json"
            summary = {"build": {"commit": GOOD[:7], "dirty": False}, "machine": IDENTITY,
                       "titles": {"PPSA00000": {"name": "Example", "version": "01.00",
                                                   "runs": [{"outcome": "rendering", "fps": 60}]}}}
            path.write_text(json.dumps(summary), encoding="utf-8")
            self.assertEqual(performance_bisect.sample_summary(path, "PPSA00000", GOOD)["fps"], 60)
            summary["build"]["commit"] = GOOD[:9]
            path.write_text(json.dumps(summary), encoding="utf-8")
            self.assertEqual(performance_bisect.sample_summary(path, "PPSA00000", GOOD)["fps"], 60)
            summary["build"]["commit"] = GOOD[:7]
            summary["build"]["dirty"] = True
            path.write_text(json.dumps(summary), encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "not the requested clean commit"):
                performance_bisect.sample_summary(path, "PPSA00000", GOOD)

    def test_sample_rejects_non_rendering_and_non_finite_fps(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "summary.json"
            summary = {"build": {"commit": GOOD[:7], "dirty": False}, "machine": IDENTITY,
                       "titles": {"PPSA00000": {"name": "Example", "version": "01.00",
                                                   "runs": [{"outcome": "stalled", "fps": 60}]}}}
            for outcome, fps in (("stalled", 60), ("rendering", math.inf), ("rendering", 0)):
                summary["titles"]["PPSA00000"]["runs"] = [{"outcome": outcome, "fps": fps}]
                path.write_text(json.dumps(summary), encoding="utf-8")
                with self.subTest(outcome=outcome, fps=fps), self.assertRaisesRegex(ValueError, "did not render"):
                    performance_bisect.sample_summary(path, "PPSA00000", GOOD)


class MeasurementStabilityTests(unittest.TestCase):
    def test_summarizes_repeated_fps_with_median(self):
        samples = [{"fps": value, "identity": IDENTITY} for value in (60.0, 61.0, 59.0)]
        result = performance_bisect.summarize(samples, 5)
        self.assertEqual(result["median_fps"], 60.0)
        self.assertAlmostEqual(result["spread_pct"], 200 / 60)

    def test_rejects_noisy_samples_and_identity_drift(self):
        for samples, limit in (([], 5), ([{"fps": value, "identity": IDENTITY} for value in (50, 60, 70)], 5),
                               ([{"fps": 60, "identity": IDENTITY}, {"fps": 60, "identity": dict(IDENTITY, gpu="Other")}], 5)):
            with self.subTest(samples=samples), self.assertRaises(ValueError):
                performance_bisect.summarize(samples, limit)

    def test_regression_requires_the_declared_slowdown_threshold(self):
        self.assertFalse(performance_bisect.is_regression(58, 60, 5))
        self.assertTrue(performance_bisect.is_regression(57, 60, 5))

    def test_candidate_identity_must_match_baseline(self):
        with self.assertRaisesRegex(ValueError, "identity changed"):
            performance_bisect.require_same_identity({"identity": IDENTITY}, {"identity": dict(IDENTITY, driver="Other")})

class GitHistoryTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.repo = Path(self.directory.name) / "repo"
        self.repo.mkdir()
        subprocess.run(["git", "init", "-q", str(self.repo)], check=True)
        subprocess.run(["git", "-C", str(self.repo), "config", "user.name", "Test"], check=True)
        subprocess.run(["git", "-C", str(self.repo), "config", "user.email", "test@example.invalid"], check=True)
        self.commit("zero")
        self.good = performance_bisect.git(self.repo, "rev-parse", "HEAD")
        self.commits = [self.commit(f"step-{index}") for index in range(1, 8)]

    def commit(self, text):
        (self.repo / "state").write_text(text, encoding="utf-8")
        subprocess.run(["git", "-C", str(self.repo), "add", "state"], check=True)
        subprocess.run(["git", "-C", str(self.repo), "commit", "-qm", f"test: {text}"], check=True)
        return performance_bisect.git(self.repo, "rev-parse", "HEAD")

    def test_resolves_first_parent_range_and_checks_ancestry(self):
        good, bad, commits = performance_bisect.first_parent_commits(self.repo, self.good, self.commits[-1])
        self.assertEqual(good, self.good)
        self.assertEqual(bad, self.commits[-1])
        self.assertEqual(commits, self.commits)
        with self.assertRaisesRegex(ValueError, "ancestor"):
            performance_bisect.first_parent_commits(self.repo, self.commits[-1], self.good)



class CommandTests(unittest.TestCase):
    def test_command_placeholders_expand_as_literal_arguments(self):
        values = {"worktree": Path("/tmp/source"), "build_dir": Path("/tmp/source/build"),
                  "result_dir": Path("/tmp/results"), "commit": "abc123"}
        command = ["cmake", "--build", "{build_dir}", "literal;argument", "{commit}"]
        self.assertEqual(performance_bisect.format_command(command, values),
                         ["cmake", "--build", str(Path("/tmp/source/build")), "literal;argument", "abc123"])

    def test_build_commands_expand_paths_without_a_shell(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / "source"
            source.mkdir()
            subprocess.run(["git", "init", "-q", str(source)], check=True)
            subprocess.run(["git", "-C", str(source), "config", "user.name", "Test"], check=True)
            subprocess.run(["git", "-C", str(source), "config", "user.email", "test@example.invalid"], check=True)
            (source / "source.cpp").write_text("source", encoding="utf-8")
            subprocess.run(["git", "-C", str(source), "add", "source.cpp"], check=True)
            subprocess.run(["git", "-C", str(source), "commit", "-qm", "test: source"], check=True)
            build_dir = root / "source" / "build"
            recipe = {"build_dir": "build", "build_commands": [[sys.executable, "-c",
                       "from pathlib import Path; Path(r'{build_dir}').mkdir(parents=True)"]]}
            actual = performance_bisect.run_build_commands(source, recipe, GOOD, root / "results")
            self.assertEqual(actual, build_dir)
            self.assertTrue(build_dir.is_dir())



class TitleRunIntegrationTests(unittest.TestCase):
    def test_title_runner_sample_is_parsed_from_its_output_directory(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            tools = root / "tools"
            tools.mkdir()
            fake_runner = tools / "title_run.py"
            fake_runner.write_text("""import json, pathlib, subprocess, sys
args = sys.argv[1:]
out = pathlib.Path(args[args.index('--out') + 1])
build = pathlib.Path(args[args.index('--build') + 1])
commit = subprocess.check_output(['git', '-C', str(build.parent), 'rev-parse', '--short', 'HEAD'], text=True).strip()
target = out / 'run' / 'summary.json'
target.parent.mkdir(parents=True)
target.write_text(json.dumps({'build': {'commit': commit, 'dirty': False}, 'machine': {'os': 'Linux', 'gpu': 'GPU', 'driver': 'Driver', 'cpu': 'CPU'}, 'titles': {'PPSA00000': {'name': 'Example', 'version': '01.00', 'runs': [{'outcome': 'rendering', 'fps': 60.0}]}}}))
""", encoding="utf-8")
            repo = root / "repo"
            repo.mkdir()
            subprocess.run(["git", "init", "-q", str(repo)], check=True)
            subprocess.run(["git", "-C", str(repo), "config", "user.name", "Test"], check=True)
            subprocess.run(["git", "-C", str(repo), "config", "user.email", "test@example.invalid"], check=True)
            (repo / "source").write_text("source", encoding="utf-8")
            subprocess.run(["git", "-C", str(repo), "add", "source"], check=True)
            subprocess.run(["git", "-C", str(repo), "commit", "-qm", "test: source"], check=True)
            commit = subprocess.check_output(["git", "-C", str(repo), "rev-parse", "HEAD"], text=True).strip()
            game = root / "game"
            game.mkdir()
            titles = root / "titles.json"
            titles.write_text("{}", encoding="utf-8")
            recipe = {"title_id": "PPSA00000", "games": [str(game)], "titles": str(titles), "seconds": 20, "content": None}
            result_dir = root / "sample"
            with patch.object(performance_bisect, "TOOLS_DIR", tools):
                sample = performance_bisect.run_sample(repo, repo / "build", result_dir, recipe, commit)
            self.assertEqual(sample["fps"], 60)
            self.assertEqual(sample["identity"]["gpu"], "GPU")
            self.assertTrue((result_dir / "run" / "summary.json").is_file())



class CommitMeasurementTests(unittest.TestCase):
    def test_measures_every_configured_run_at_the_requested_commit(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            worktree = root / "worktree"
            worktree.mkdir()
            results = root / "results"
            results.mkdir()
            recipe = {"repetitions": 3, "max_spread_pct": 5}
            sample = {"fps": 60.0, "identity": IDENTITY}
            with patch.object(performance_bisect, "git") as git, \
                    patch.object(performance_bisect, "run_build_commands", return_value=worktree / "build") as build, \
                    patch.object(performance_bisect, "run_sample", return_value=sample) as run:
                measurement = performance_bisect.measure_commit("repo", worktree, results, recipe, GOOD)
            git.assert_called_once_with(worktree, "checkout", "--detach", "--force", GOOD)
            build.assert_called_once_with(worktree, recipe, GOOD, results / GOOD)
            self.assertEqual(run.call_count, 3)
            self.assertEqual(measurement["median_fps"], 60.0)
            self.assertTrue((results / GOOD).is_dir())


class ReportTests(unittest.TestCase):
    def test_report_is_written_once_as_complete_json(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "nested" / "report.json"
            performance_bisect.write_report(path, {"result": "first bad"})
            self.assertEqual(json.loads(path.read_text(encoding="utf-8")), {"result": "first bad"})
            with self.assertRaisesRegex(ValueError, "already exists"):
                performance_bisect.write_report(path, {"result": "overwrite"})
            self.assertEqual(json.loads(path.read_text(encoding="utf-8")), {"result": "first bad"})


class EndpointValidationTests(unittest.TestCase):
    def test_requires_slow_endpoint_and_matching_measurement_identity(self):
        baseline = {"median_fps": 60.0, "identity": IDENTITY}
        self.assertIsNone(performance_bisect.validate_regression_endpoints(
            baseline, {"median_fps": 57.0, "identity": IDENTITY}, 5.0))
        with self.assertRaisesRegex(ValueError, "bad endpoint"):
            performance_bisect.validate_regression_endpoints(
                baseline, {"median_fps": 59.0, "identity": IDENTITY}, 5.0)
        with self.assertRaisesRegex(ValueError, "identity changed"):
            performance_bisect.validate_regression_endpoints(
                baseline, {"median_fps": 50.0, "identity": dict(IDENTITY, gpu="Other")}, 5.0)


class BisectSearchTests(unittest.TestCase):
    def test_binary_search_returns_the_first_bad_and_previous_good_commit(self):
        commits = [f"commit-{index}" for index in range(1, 8)]
        baseline = {"median_fps": 60.0, "identity": IDENTITY}
        values = {commit: 60.0 for commit in commits}
        values.update({commit: 54.0 for commit in commits[4:]})

        def measure(repo, worktree, results, recipe, commit):
            return {"median_fps": values[commit], "identity": IDENTITY, "samples": []}

        with patch.object(performance_bisect, "measure_commit", side_effect=measure) as measured:
            first_bad, previous_good, samples = performance_bisect.find_first_bad_commit(
                "repo", Path("worktree"), Path("results"), {"regression_pct": 5},
                "good", baseline, "bad", {"median_fps": 54.0, "identity": IDENTITY}, commits)
        self.assertEqual(first_bad, commits[4])
        self.assertEqual(previous_good, commits[3])
        self.assertLessEqual(measured.call_count, 4)
        self.assertIn(previous_good, samples)


class BisectReportTests(unittest.TestCase):
    def test_report_documents_assumption_and_orders_measured_commits(self):
        good, first_bad, previous_good, bad = "good", "bad-2", "bad-1", "bad-3"
        commits = ["bad-1", "bad-2", "bad-3"]
        values = {commit: {"median_fps": fps, "identity": IDENTITY}
                  for commit, fps in ((good, 60.0), (previous_good, 59.0),
                                      (first_bad, 54.0), (bad, 53.0))}
        report = performance_bisect.make_report(
            good, bad, commits, first_bad, previous_good, values,
            {"regression_pct": 5.0, "max_spread_pct": 3.0})
        self.assertEqual(report["method"], "binary search over first-parent commits")
        self.assertIn("monotonic", report["assumption"])
        self.assertEqual([row["commit"] for row in report["measurements"]], commits)
        self.assertEqual(report["first_bad"]["median_fps"], 54.0)


class EndToEndBisectTests(GitHistoryTests):
    def test_bisects_to_first_commit_over_threshold_and_removes_worktree(self):
        values = {commit: 60.0 for commit in [self.good, *self.commits]}
        values.update({commit: 54.0 for commit in self.commits[4:]})

        def fake_measure(repo, worktree, results, recipe, commit):
            return {"median_fps": values[commit], "spread_pct": 0.0,
                    "samples": [{"fps": values[commit], "identity": IDENTITY}], "identity": IDENTITY}

        recipe = {"regression_pct": 5.0, "max_spread_pct": 3.0}
        with patch.object(performance_bisect, "measure_commit", side_effect=fake_measure):
            report = performance_bisect.bisect(self.repo, self.good, self.commits[-1], recipe, None)
        self.assertEqual(report["first_bad"]["commit"], self.commits[4])
        self.assertEqual(report["previous_good"]["commit"], self.commits[3])
        self.assertEqual(report["tested_commits"], 5)
        measured_commits = [entry["commit"] for entry in report["measurements"]]
        self.assertEqual(measured_commits, [commit for commit in self.commits if commit in measured_commits])
        self.assertEqual(len(measured_commits), report["tested_commits"] - 1)
        self.assertEqual(performance_bisect.git(self.repo, "worktree", "list").count("source"), 0)

    def test_rejects_bad_endpoint_without_creating_worktree(self):
        recipe = {"regression_pct": 5.0, "max_spread_pct": 3.0}
        with patch.object(performance_bisect, "measure_commit", side_effect=[
                {"median_fps": 60.0, "identity": IDENTITY}, {"median_fps": 59.0, "identity": IDENTITY}]), \
                self.assertRaisesRegex(ValueError, "bad endpoint"):
            performance_bisect.bisect(self.repo, self.good, self.commits[-1], recipe, None)



class CliParserTests(unittest.TestCase):
    def test_help_explains_recipe_safety_and_bisect_assumptions(self):
        help_text = performance_bisect.build_parser().format_help()
        self.assertIn("--good", help_text)
        self.assertIn("without a shell", help_text)
        self.assertIn("monotonic regression", help_text)
        self.assertIn("max_spread_pct", help_text)


class CliExecutionTests(unittest.TestCase):
    def test_main_reports_the_first_bad_and_good_commit(self):
        report = {"first_bad": {"commit": "bad", "median_fps": 54.0},
                  "previous_good": {"commit": "good", "median_fps": 60.0}}
        with patch.object(performance_bisect, "load_recipe", return_value={}), \
                patch.object(performance_bisect, "bisect", return_value=report) as bisect, \
                patch("sys.stdout", new_callable=__import__("io").StringIO) as stdout:
            result = performance_bisect.main(["--good", "good", "--bad", "bad", "--recipe", "recipe.json"])
        self.assertEqual(result, 0)
        self.assertIn("first measured regression: bad (54.00 FPS)", stdout.getvalue())
        bisect.assert_called_once()

    def test_main_reports_a_non_ascii_output_path_as_utf8(self):
        from io import BytesIO, TextIOWrapper
        report = {"first_bad": {"commit": "bad", "median_fps": 54.0},
                  "previous_good": {"commit": "good", "median_fps": 60.0}}
        output = Path(tempfile.gettempdir()) / "界" / "report.json"
        stdout = TextIOWrapper(BytesIO(), encoding="cp1252")
        with patch.object(performance_bisect, "load_recipe", return_value={}), \
                patch.object(performance_bisect, "bisect", return_value=report), patch("sys.stdout", stdout):
            result = performance_bisect.main(["--good", "good", "--bad", "bad", "--recipe", "recipe.json", "--output", str(output)])
            stdout.flush()
        self.assertEqual(result, 0)
        self.assertIn(f"report written to {output}", stdout.buffer.getvalue().decode("utf-8"))

    def test_main_rejects_a_missing_temporary_directory(self):
        from contextlib import redirect_stderr
        from io import StringIO
        stderr = StringIO()
        with redirect_stderr(stderr), self.assertRaises(SystemExit) as error:
            performance_bisect.main(["--good", "good", "--bad", "bad", "--recipe", "recipe.json",
                                     "--temp-dir", "/path/that/does/not/exist"])
        self.assertEqual(error.exception.code, 2)
        self.assertIn("--temp-dir must name an existing directory", stderr.getvalue())


class CleanBuildTests(unittest.TestCase):
    def test_removes_stale_build_outputs_before_configuring_a_revision(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            repo = root / "repo"
            repo.mkdir()
            subprocess.run(["git", "init", "-q", str(repo)], check=True)
            subprocess.run(["git", "-C", str(repo), "config", "user.name", "Test"], check=True)
            subprocess.run(["git", "-C", str(repo), "config", "user.email", "test@example.invalid"], check=True)
            (repo / "source.cpp").write_text("source", encoding="utf-8")
            subprocess.run(["git", "-C", str(repo), "add", "source.cpp"], check=True)
            subprocess.run(["git", "-C", str(repo), "commit", "-qm", "test: source"], check=True)
            stale = repo / "build" / "stale.txt"
            stale.parent.mkdir()
            stale.write_text("previous commit", encoding="utf-8")
            command = [sys.executable, "-c", "from pathlib import Path; p=Path(r'{build_dir}'); assert not (p/'stale.txt').exists(); p.mkdir()"]
            result = performance_bisect.run_build_commands(
                repo, {"build_dir": "build", "build_commands": [command]}, GOOD, root / "results")
            self.assertEqual(result, repo / "build")
            self.assertFalse(stale.exists())
            self.assertTrue(result.is_dir())


class BuildPathSafetyTests(unittest.TestCase):
    def make_repo(self, root):
        repo = root / "repo"
        repo.mkdir()
        subprocess.run(["git", "init", "-q", str(repo)], check=True)
        subprocess.run(["git", "-C", str(repo), "config", "user.name", "Test"], check=True)
        subprocess.run(["git", "-C", str(repo), "config", "user.email", "test@example.invalid"], check=True)
        return repo

    def test_refuses_to_clear_a_build_directory_with_tracked_source(self):
        with tempfile.TemporaryDirectory() as directory:
            repo = self.make_repo(Path(directory))
            tracked = repo / "build" / "source.cpp"
            tracked.parent.mkdir()
            tracked.write_text("tracked source", encoding="utf-8")
            subprocess.run(["git", "-C", str(repo), "add", "build/source.cpp"], check=True)
            subprocess.run(["git", "-C", str(repo), "commit", "-qm", "test: tracked build source"], check=True)
            with self.assertRaisesRegex(ValueError, "contains tracked source"):
                performance_bisect.reset_build_dir(repo, repo / "build")
            self.assertEqual(tracked.read_text(encoding="utf-8"), "tracked source")


class BuildSymlinkSafetyTests(unittest.TestCase):
    def test_refuses_to_follow_a_build_directory_symlink_outside_the_worktree(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            repo = root / "repo"
            repo.mkdir()
            subprocess.run(["git", "init", "-q", str(repo)], check=True)
            external = root / "external"
            external.mkdir()
            marker = external / "keep.txt"
            marker.write_text("outside data", encoding="utf-8")
            (repo / "build").symlink_to(external, target_is_directory=True)
            with self.assertRaisesRegex(ValueError, "resolves outside the worktree"):
                performance_bisect.reset_build_dir(repo, repo / "build")
            self.assertEqual(marker.read_text(encoding="utf-8"), "outside data")
            self.assertTrue((repo / "build").is_symlink())


class CliEntrypointTests(unittest.TestCase):
    def test_script_help_runs_without_a_recipe_or_title_data(self):
        result = subprocess.run([sys.executable, str(Path(performance_bisect.__file__)), "--help"],
                                capture_output=True, text=True, check=False)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("known-good commit", result.stdout)
        self.assertIn("known-bad commit", result.stdout)


class FirstParentMergeTests(GitHistoryTests):
    def test_excludes_side_branch_commits_and_keeps_the_merge_commit(self):
        default_branch = performance_bisect.git(self.repo, "symbolic-ref", "--short", "HEAD")
        performance_bisect.git(self.repo, "switch", "-c", "bisect-side")
        (self.repo / "side.txt").write_text("side-only change", encoding="utf-8")
        subprocess.run(["git", "-C", str(self.repo), "add", "side.txt"], check=True)
        subprocess.run(["git", "-C", str(self.repo), "commit", "-qm", "test: side branch"], check=True)
        side_commit = performance_bisect.git(self.repo, "rev-parse", "HEAD")
        performance_bisect.git(self.repo, "switch", default_branch)
        (self.repo / "main.txt").write_text("first-parent change", encoding="utf-8")
        subprocess.run(["git", "-C", str(self.repo), "add", "main.txt"], check=True)
        subprocess.run(["git", "-C", str(self.repo), "commit", "-qm", "test: first parent"], check=True)
        first_parent_change = performance_bisect.git(self.repo, "rev-parse", "HEAD")
        subprocess.run(["git", "-C", str(self.repo), "merge", "--no-ff", "bisect-side",
                        "-m", "merge: side branch"], check=True, capture_output=True)
        merge_commit = performance_bisect.git(self.repo, "rev-parse", "HEAD")

        good, bad, commits = performance_bisect.first_parent_commits(self.repo, self.good, merge_commit)

        self.assertEqual(good, self.good)
        self.assertEqual(bad, merge_commit)
        self.assertIn(first_parent_change, commits)
        self.assertIn(merge_commit, commits)
        self.assertNotIn(side_commit, commits)
        self.assertEqual(len(commits), len(set(commits)))


class BisectCleanupTests(GitHistoryTests):
    def test_removes_temporary_worktree_when_a_measurement_fails(self):
        valid = {"median_fps": 60.0, "identity": IDENTITY}
        bad = {"median_fps": 54.0, "identity": IDENTITY}
        recipe = {"regression_pct": 5.0, "max_spread_pct": 3.0}
        with patch.object(performance_bisect, "measure_commit", side_effect=[valid, bad, RuntimeError("build failed")]), \
                self.assertRaisesRegex(RuntimeError, "build failed"):
            performance_bisect.bisect(self.repo, self.good, self.commits[-1], recipe, None)
        worktrees = performance_bisect.git(self.repo, "worktree", "list")
        self.assertEqual(worktrees.count("source"), 0)


class AtomicReportRaceTests(unittest.TestCase):
    def test_does_not_overwrite_a_report_created_during_write(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "report.json"
            link = performance_bisect.os.link

            def create_racing_report(source, destination):
                Path(destination).write_text("owner report", encoding="utf-8")
                return link(source, destination)

            with patch.object(performance_bisect.os, "link", side_effect=create_racing_report), \
                    self.assertRaises(FileExistsError):
                performance_bisect.write_report(path, {"result": "bisect report"})
            self.assertEqual(path.read_text(encoding="utf-8"), "owner report")
            self.assertEqual(list(Path(directory).iterdir()), [path])


class FirstParentEndpointTests(GitHistoryTests):
    def test_rejects_good_endpoint_reachable_only_through_a_merge_parent(self):
        default_branch = performance_bisect.git(self.repo, "symbolic-ref", "--short", "HEAD")
        performance_bisect.git(self.repo, "switch", "-c", "endpoint-side")
        (self.repo / "side-endpoint.txt").write_text("side", encoding="utf-8")
        subprocess.run(["git", "-C", str(self.repo), "add", "side-endpoint.txt"], check=True)
        subprocess.run(["git", "-C", str(self.repo), "commit", "-qm", "test: side endpoint"], check=True)
        side_good = performance_bisect.git(self.repo, "rev-parse", "HEAD")
        performance_bisect.git(self.repo, "switch", default_branch)
        (self.repo / "first-parent.txt").write_text("main", encoding="utf-8")
        subprocess.run(["git", "-C", str(self.repo), "add", "first-parent.txt"], check=True)
        subprocess.run(["git", "-C", str(self.repo), "commit", "-qm", "test: first parent"], check=True)
        subprocess.run(["git", "-C", str(self.repo), "merge", "--no-ff", "endpoint-side",
                        "-m", "merge: side endpoint"], check=True, capture_output=True)
        bad = performance_bisect.git(self.repo, "rev-parse", "HEAD")
        with self.assertRaisesRegex(ValueError, "first-parent history"):
            performance_bisect.first_parent_commits(self.repo, side_good, bad)


class FocusedInputEdgeTests(unittest.TestCase):
    def recipe_path(self, root, **updates):
        recipe = RecipeTests().make_recipe(root)
        recipe.update(updates)
        path = root / "recipe.json"
        path.write_text(json.dumps(recipe), encoding="utf-8")
        return path

    def test_recipe_root_must_be_an_object(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "recipe.json"
            path.write_text("[]", encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "JSON object"):
                performance_bisect.load_recipe(path)

    def write_summary(self, root, summary):
        path = root / "summary.json"
        path.write_text(json.dumps(summary), encoding="utf-8")
        return path

    def sample_fixture(self):
        return {"build": {"commit": GOOD[:12], "dirty": False},
                "machine": {"os": "Linux", "gpu": "GPU", "driver": "Driver", "cpu": "CPU"},
                "titles": {"PPSA00000": {"name": "Example", "version": "01.00",
                                         "runs": [{"outcome": "rendering", "fps": 60.0}]}}}

    def test_sample_rejects_dirty_build_identity(self):
        with tempfile.TemporaryDirectory() as directory:
            summary = self.sample_fixture()
            summary["build"]["dirty"] = True
            path = self.write_summary(Path(directory), summary)
            with self.assertRaisesRegex(ValueError, "requested clean commit"):
                performance_bisect.sample_summary(path, "PPSA00000", GOOD)


    def test_recipe_rejects_empty_build_arguments(self):
        with tempfile.TemporaryDirectory() as directory:
            path = self.recipe_path(Path(directory), build_commands=[["cmake", ""]])
            with self.assertRaisesRegex(ValueError, "argument list"):
                performance_bisect.load_recipe(path)


    def test_recipe_rejects_absolute_build_directories(self):
        with tempfile.TemporaryDirectory() as directory:
            path = self.recipe_path(Path(directory), build_dir="/tmp/unsafe-build")
            with self.assertRaisesRegex(ValueError, "relative path"):
                performance_bisect.load_recipe(path)


    @unittest.skipUnless(sys.platform == "win32", "drive-relative paths exist only on Windows")
    def test_recipe_rejects_drive_relative_build_directories(self):
        with tempfile.TemporaryDirectory() as directory:
            path = self.recipe_path(Path(directory), build_dir="D:unsafe-build")
            with self.assertRaisesRegex(ValueError, "relative path"):
                performance_bisect.load_recipe(path)


    def test_recipe_rejects_worktree_root_build_directory(self):
        with tempfile.TemporaryDirectory() as directory:
            path = self.recipe_path(Path(directory), build_dir=".")
            with self.assertRaisesRegex(ValueError, "non-root"):
                performance_bisect.load_recipe(path)


    def test_recipe_rejects_missing_game_directories(self):
        with tempfile.TemporaryDirectory() as directory:
            path = self.recipe_path(Path(directory), games=["missing-game"])
            with self.assertRaisesRegex(ValueError, "not an existing directory"):
                performance_bisect.load_recipe(path)


    def test_recipe_rejects_boolean_duration(self):
        with tempfile.TemporaryDirectory() as directory:
            path = self.recipe_path(Path(directory), seconds=True)
            with self.assertRaisesRegex(ValueError, "integer of at least 20"):
                performance_bisect.load_recipe(path)


    def test_recipe_rejects_boolean_repetitions(self):
        with tempfile.TemporaryDirectory() as directory:
            path = self.recipe_path(Path(directory), repetitions=True)
            with self.assertRaisesRegex(ValueError, "integer of at least 3"):
                performance_bisect.load_recipe(path)


    def test_recipe_requires_regression_threshold_inside_open_interval(self):
        with tempfile.TemporaryDirectory() as directory:
            for value in (0, 100):
                with self.subTest(value=value):
                    case = Path(directory) / str(value)
                    case.mkdir()
                    path = self.recipe_path(case, regression_pct=value)
                    with self.assertRaisesRegex(ValueError, "between 0 and 100"):
                        performance_bisect.load_recipe(path)


    def test_recipe_requires_spread_limit_inside_open_interval(self):
        with tempfile.TemporaryDirectory() as directory:
            for value in (0, 100):
                with self.subTest(value=value):
                    case = Path(directory) / str(value)
                    case.mkdir()
                    path = self.recipe_path(case, max_spread_pct=value)
                    with self.assertRaisesRegex(ValueError, "between 0 and 100"):
                        performance_bisect.load_recipe(path)


    def test_recipe_rejects_missing_content_directory(self):
        with tempfile.TemporaryDirectory() as directory:
            path = self.recipe_path(Path(directory), content="missing-content")
            with self.assertRaisesRegex(ValueError, "existing content directory"):
                performance_bisect.load_recipe(path)


    def test_recipe_rejects_malformed_json(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "recipe.json"
            path.write_text("{", encoding="utf-8")
            with self.assertRaises(json.JSONDecodeError):
                performance_bisect.load_recipe(path)


    def test_sample_rejects_multiple_title_runs(self):
        with tempfile.TemporaryDirectory() as directory:
            summary = self.sample_fixture()
            run = summary["titles"]["PPSA00000"]["runs"][0]
            summary["titles"]["PPSA00000"]["runs"] = [run, dict(run)]
            path = self.write_summary(Path(directory), summary)
            with self.assertRaisesRegex(ValueError, "exactly one"):
                performance_bisect.sample_summary(path, "PPSA00000", GOOD)


    def test_sample_requires_gpu_identity(self):
        with tempfile.TemporaryDirectory() as directory:
            summary = self.sample_fixture()
            del summary["machine"]["gpu"]
            path = self.write_summary(Path(directory), summary)
            with self.assertRaisesRegex(ValueError, "missing machine"):
                performance_bisect.sample_summary(path, "PPSA00000", GOOD)


    def test_summary_uses_middle_pair_for_even_sample_count(self):
        samples = [{"fps": value, "identity": IDENTITY} for value in (60.0, 62.0)]
        result = performance_bisect.summarize(samples, 5)
        self.assertEqual(result["median_fps"], 61.0)


    def test_summary_accepts_spread_equal_to_limit(self):
        samples = [{"fps": value, "identity": IDENTITY} for value in (59.0, 61.0)]
        spread = (61.0 - 59.0) / 60.0 * 100
        result = performance_bisect.summarize(samples, spread)
        self.assertEqual(result["spread_pct"], spread)
        with self.assertRaisesRegex(ValueError, "exceeds"):
            performance_bisect.summarize(samples, math.nextafter(spread, 0.0))


    def test_summary_rejects_content_version_drift(self):
        samples = [{"fps": 60.0, "identity": IDENTITY},
                   {"fps": 60.0, "identity": dict(IDENTITY, content_version="02.00")}]
        with self.assertRaisesRegex(ValueError, "identity changed"):
            performance_bisect.summarize(samples, 5)


    def test_first_parent_range_rejects_unrelated_bad_commit(self):
        with tempfile.TemporaryDirectory() as directory:
            repo = Path(directory) / "repo"
            repo.mkdir()
            subprocess.run(["git", "init", "-q", str(repo)], check=True)
            subprocess.run(["git", "-C", str(repo), "config", "user.name", "Test"], check=True)
            subprocess.run(["git", "-C", str(repo), "config", "user.email", "test@example.invalid"], check=True)
            (repo / "base").write_text("base", encoding="utf-8")
            subprocess.run(["git", "-C", str(repo), "add", "base"], check=True)
            subprocess.run(["git", "-C", str(repo), "commit", "-qm", "test: base"], check=True)
            good = performance_bisect.git(repo, "rev-parse", "HEAD")
            performance_bisect.git(repo, "switch", "--orphan", "unrelated")
            (repo / "other").write_text("other", encoding="utf-8")
            subprocess.run(["git", "-C", str(repo), "add", "other"], check=True)
            subprocess.run(["git", "-C", str(repo), "commit", "-qm", "test: unrelated"], check=True)
            bad = performance_bisect.git(repo, "rev-parse", "HEAD")
            with self.assertRaisesRegex(ValueError, "ancestor"):
                performance_bisect.first_parent_commits(repo, good, bad)


    def test_build_command_failure_reports_status_and_output(self):
        with tempfile.TemporaryDirectory() as directory:
            repo = Path(directory) / "repo"
            repo.mkdir()
            subprocess.run(["git", "init", "-q", str(repo)], check=True)
            command = [sys.executable, "-c", "print('compiler detail'); raise SystemExit(7)"]
            with self.assertRaisesRegex(RuntimeError, "failed \(7\).*compiler detail"):
                performance_bisect.run_build_commands(
                    repo, {"build_dir": "build", "build_commands": [command]},
                    GOOD, Path(directory) / "results")


    def test_run_sample_rejects_multiple_summary_files(self):
        from types import SimpleNamespace

        def fake_runner(command, **kwargs):
            output = Path(command[command.index("--out") + 1])
            for name in ("first", "second"):
                summary = output / name / "summary.json"
                summary.parent.mkdir(parents=True)
                summary.write_text("{}", encoding="utf-8")
            return SimpleNamespace(returncode=0, stdout="", stderr="")

        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            game = root / "game"
            game.mkdir()
            recipe = {"title_id": "PPSA00000", "games": [str(game)], "content": None,
                      "titles": str(root / "titles.json"), "seconds": 20}
            result = root / "result"
            with patch.object(performance_bisect, "TOOLS_DIR", root),                     patch.object(performance_bisect.subprocess, "run", side_effect=fake_runner),                     self.assertRaisesRegex(RuntimeError, "produced 2 summary files"):
                performance_bisect.run_sample(root, root / "build", result, recipe, GOOD)


    def test_run_sample_reports_title_runner_failure(self):
        from types import SimpleNamespace

        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            game = root / "game"
            game.mkdir()
            recipe = {"title_id": "PPSA00000", "games": [str(game)], "content": None,
                      "titles": str(root / "titles.json"), "seconds": 20}
            with patch.object(performance_bisect.subprocess, "run",
                              return_value=SimpleNamespace(returncode=3, stdout="", stderr="runner failed")), \
                    self.assertRaisesRegex(RuntimeError, "title run failed.*runner failed"):
                performance_bisect.run_sample(root, root / "build", root / "result", recipe, GOOD)


    def test_report_rejects_non_finite_values_without_leaving_temp_files(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "report.json"
            with self.assertRaises(ValueError):
                performance_bisect.write_report(path, {"fps": math.nan})
            self.assertFalse(path.exists())
            self.assertEqual(list(Path(directory).iterdir()), [])


    def test_bisect_refuses_existing_output_before_history_or_worktree_access(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "report.json"
            output.write_text("owned", encoding="utf-8")
            with patch.object(performance_bisect, "first_parent_commits") as history:
                with self.assertRaisesRegex(ValueError, "report already exists"):
                    performance_bisect.bisect(directory, "good", "bad", {}, output)
            history.assert_not_called()
            self.assertEqual(output.read_text(encoding="utf-8"), "owned")
