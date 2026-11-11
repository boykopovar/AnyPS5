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
                         ["cmake", "--build", "/tmp/source/build", "literal;argument", "abc123"])

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
