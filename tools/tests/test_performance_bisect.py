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
                               ("build_dir", "../outside"), ("seconds", 10),
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

