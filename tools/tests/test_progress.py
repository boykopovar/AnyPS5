import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import progress


class ProgressTests(unittest.TestCase):
    def test_shared_source_refactor_preserves_functions(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            owner = root / "libSceExample"
            alias = root / "libSceExample.native"
            empty = root / "libSceEmpty"
            for path in (owner, alias, empty):
                path.mkdir()
            source = "int APS5_VABI Ready() { return 0; }\nint APS5_VABI Pending() { NotImplemented_nid_no_patch(__func__); }\n"
            (owner / "Export.cpp").write_text(source)
            (alias / "Export.cpp").write_text(source)
            (owner / "Library.cmake").write_text("add_library(example SHARED Export.cpp)\n")
            (empty / "CMakeLists.txt").write_text('# include(${CMAKE_CURRENT_SOURCE_DIR}/../libSceExample/Library.cmake)\n')
            with patch.object(progress, "PRX", root):
                base = progress.collect_libraries()
                self.assertEqual((base["done"], base["total"]), (2, 4))
                (alias / "Export.cpp").unlink()
                (alias / "CMakeLists.txt").write_text('include("${CMAKE_CURRENT_SOURCE_DIR}/../libSceExample/Library.cmake")\n')
                head = progress.collect_libraries()
                self.assertEqual((head["done"], head["total"]), (1, 2))
                groups = {group["name"]: group for group in head["groups"]}
                self.assertEqual(groups[alias.name]["shared_sources"], owner.name)
                self.assertNotIn("shared_sources", groups[empty.name])
                html = progress.table("Libraries", "Library", head)
                self.assertIn('colspan="3">Shared sources:', html)
                self.assertIn("shared by " + alias.name, "\n".join(progress.treemap("Libraries", head, 0)))
                report = "\n".join(progress.compare("Libraries", "Library", "functions", base, head))
                self.assertIn("sharing sources", report)
                self.assertNotIn("removed", report)
                self.assertNotIn("implemented", report)
                self.assertEqual(progress.compare("Libraries", "Library", "functions", head, head), [])
                (owner / "Export.cpp").write_text(source.replace("return 0;", "NotImplemented_nid_no_patch(__func__);"))
                regressed = progress.collect_libraries()
                report = "\n".join(progress.compare("Libraries", "Library", "functions", base, regressed))
                self.assertIn("-1 reverted", report)
                self.assertIn("Ready", report)
                (owner / "Export.cpp").write_text("int APS5_VABI Ready() { return 0; }\n")
                removed = progress.collect_libraries()
                report = "\n".join(progress.compare("Libraries", "Library", "functions", base, removed))
                self.assertIn("-1 removed", report)
                self.assertIn("Pending", report)


if __name__ == "__main__":
    unittest.main()
