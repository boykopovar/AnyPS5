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

    def test_unobserved_stubs_get_their_own_colour_and_legend(self):
        libraries = {"done": 1, "total": 2, "percent": 50, "groups": [
            {"name": "libA", "label": "libA", "done": 1, "todo": 1, "unobserved": 2}]}
        parts = progress.treemap("System libraries*", libraries, 0)
        cells = "\n".join(parts)
        self.assertEqual(cells.count('fill="#2ea043"'), 1)
        self.assertEqual(cells.count('fill="#da3633"'), 1)
        self.assertEqual(cells.count('fill="#6e7681"'), 2)
        self.assertIn("libA: 1/2 (50%), 2 unobserved", cells)
        svg = progress.render(libraries, {"done": 0, "total": 0, "percent": 0, "groups": []})
        self.assertIn(">Unobserved, not known to be needed (libraries) / Not implemented (shaders)</text>", svg)
        self.assertEqual(svg.count('fill="#6e7681"'), 2 + 1)
        self.assertEqual(len({progress.DONE_COLOR, progress.TODO_COLOR, progress.UNOBSERVED_COLOR}), 3)
        self.assertFalse(hasattr(progress, "PENDING_COLOR"))

    def test_shader_panel_draws_pending_instructions_in_the_shared_grey(self):
        shaders = {"done": 1, "total": 2, "percent": 50, "groups": [{"name": "SOP1", "label": "SOP1", "done": 1, "todo": 1}]}
        cells = "\n".join(progress.treemap("GPU shader instructions", shaders, 0, states=False))
        self.assertEqual((cells.count('fill="#2ea043"'), cells.count('fill="#6e7681"'), cells.count('fill="#da3633"')), (1, 1, 0))

    def test_pending_cells_show_name_provenance_origin_and_inventory_when_known(self):
        sha = "0123456789abcdef0123456789abcdef01234567"
        libraries = {"done": 1, "total": 2, "percent": 50, "groups": [{
            "name": "libA", "label": "libA", "done": 1, "todo": 1, "unobserved": 1, "done_names": ["Ready"],
            "todo_names": ["Seen"], "unobserved_names": ["Unseen"], "observed_tags": {"Seen": "loader"},
            "origins": {"Seen": {"commit": sha, "date": "2026-01-02"}, "Unseen": {"commit": sha, "date": "2026-03-04"}},
            "inventory": {"Unseen": "Libs-3.20"}}]}
        cells = "\n".join(progress.treemap("System libraries*", libraries, 0, detail=True))
        self.assertIn("<title>Unseen: unobserved stub, declared in 0123456789 on 2026-03-04, listed in Libs-3.20</title>", cells)
        self.assertIn("<title>Seen: observed stub, observed by loader, declared in 0123456789 on 2026-01-02</title>", cells)
        self.assertNotIn("Ready:", cells)

    def test_the_plain_map_has_no_per_cell_tooltips_and_the_detailed_one_does(self):
        libraries = {"done": 0, "total": 1, "percent": 0, "groups": [{
            "name": "libA", "label": "libA", "done": 0, "todo": 1, "unobserved": 1, "todo_names": ["Seen"], "unobserved_names": ["Unseen"],
            "origins": {"Unseen": {"commit": "a" * 40, "date": "2026-01-01"}}}]}
        shaders = {"done": 0, "total": 0, "percent": 0, "groups": []}
        self.assertNotIn("Unseen:", progress.render(libraries, shaders))
        self.assertIn("Unseen: unobserved stub", progress.render(libraries, shaders, detail=True))
        self.assertTrue(progress.traced(libraries))
        self.assertFalse(progress.traced({"groups": [{"name": "libA"}]}))

    def test_cells_get_no_tooltip_without_traceability_data(self):
        libraries = {"done": 0, "total": 1, "percent": 0, "groups": [{
            "name": "libA", "label": "libA", "done": 0, "todo": 1, "unobserved": 1, "todo_names": ["Seen"], "unobserved_names": ["Unseen"]}]}
        cells = "\n".join(progress.treemap("System libraries*", libraries, 0))
        self.assertNotIn("Seen:", cells)
        self.assertNotIn("Unseen:", cells)

    def test_cells_without_area(self):
        self.assertEqual(progress.cells(5, 0, 0, 0, 10), [])
        self.assertEqual(progress.cells(5, 0, 0, -1, 10), [])


if __name__ == "__main__":
    unittest.main()
