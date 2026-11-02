import importlib.util
import unittest
from pathlib import Path

SPEC = importlib.util.spec_from_file_location("progress", Path(__file__).resolve().parents[1] / "progress.py")
progress = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(progress)


class ProgressRenderSliverTests(unittest.TestCase):
    def test_render_with_sliver_group(self):
        libraries = {"done": 301, "total": 301, "percent": 100, "groups": [
            {"name": "libA", "label": "libA", "done": 300, "todo": 0},
            {"name": "libB", "label": "libB", "done": 1, "todo": 0}]}
        shaders = {"done": 0, "total": 0, "percent": 0, "groups": []}
        svg = progress.render(libraries, shaders)
        self.assertIn("<svg", svg)
        self.assertIn(">libA</text>", svg)
        self.assertNotIn(">lib</text>", svg)

    def test_cells_without_area(self):
        self.assertEqual(progress.cells(5, 0, 0, 0, 10), [])
        self.assertEqual(progress.cells(5, 0, 0, -1, 10), [])


if __name__ == "__main__":
    unittest.main()
