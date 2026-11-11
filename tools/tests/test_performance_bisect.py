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

