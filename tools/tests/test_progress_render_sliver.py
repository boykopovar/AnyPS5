import importlib.util
import re
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

    SHADERS = {"done": 1, "total": 2, "percent": 50.0, "groups": [{"name": "SOP1", "label": "SOP1", "done": 1, "todo": 1}]}

    def many_libraries(self):
        groups = [{"name": "libBig", "label": "Big", "done": 5000, "todo": 5000}]
        groups += [{"name": f"libMid{i}", "label": f"Mid{i}", "done": 20, "todo": 20} for i in range(40)]
        groups += [{"name": f"libSmall{i}", "label": f"Small{i}", "done": i % 2, "todo": 1} for i in range(100)]
        groups.append({"name": "libAlias", "label": "Alias", "done": 0, "todo": 0, "shared_sources": "libSmall7"})
        return progress.summarize(groups)

    def test_every_library_gets_one_labelled_tile_in_one_tier(self):
        libraries = self.many_libraries()
        tiers = progress.library_tiers(libraries)
        self.assertEqual(len(tiers), 3)
        names = [g["name"] for tier in tiers for g in progress.map_groups(tier)]
        self.assertEqual(sorted(names), sorted(g["name"] for g in progress.map_groups(libraries)))
        self.assertEqual(sum(tier["total"] for tier in tiers), libraries["total"])
        svg = progress.render(libraries, self.SHADERS)
        for title, tier in zip(progress.LIBRARY_TIERS, tiers):
            self.assertIn(f'>{title}: {tier["percent"]}% ({tier["done"]}/{tier["total"]})</text>', svg)
        self.assertIn(f'>System libraries*: {libraries["percent"]}% ({libraries["done"]}/{libraries["total"]})</text>', svg)
        for name in names:
            self.assertEqual(svg.count(f"<title>{name}: "), 1)
        self.assertEqual(svg.count("<g><title>lib"), svg.count("</text>\n</g>", 0, svg.index("GPU shader instructions")))
        self.assertIn("<title>libSmall7: 1/2 (50%); shared by libAlias</title>", svg)

    def test_small_tier_is_a_grid_in_name_order(self):
        tiers = progress.library_tiers(self.many_libraries())
        grid, height = progress.tile_grid("Small system libraries", tiers[2], 0)
        labels = re.findall(r'paint-order="stroke">(Small\d+)</text>', "\n".join(grid))
        self.assertEqual(labels, sorted(labels, key=str.lower))
        self.assertEqual(height, progress.HEADER + -(-len(labels) // progress.GRID_COLUMNS) * progress.GRID_ROW)

    def test_few_libraries_draw_no_empty_tiers(self):
        libraries = progress.summarize([{"name": "libBig", "label": "Big", "done": 600, "todo": 0},
                                        {"name": "libSmall", "label": "Small", "done": 0, "todo": 400}])
        self.assertEqual(len(progress.library_tiers(libraries)), 1)
        svg = progress.render(libraries, self.SHADERS)
        self.assertIn(">Large system libraries: 60.0% (600/1000)</text>", svg)
        self.assertNotIn("Medium system libraries", svg)
        self.assertNotIn("Small system libraries", svg)

    def test_cells_without_area(self):
        self.assertEqual(progress.cells(5, 0, 0, 0, 10), [])
        self.assertEqual(progress.cells(5, 0, 0, -1, 10), [])


if __name__ == "__main__":
    unittest.main()
