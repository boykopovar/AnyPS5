import os
import subprocess
import sys
import tempfile
import unittest
from datetime import date
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import progress
import progress_history

OPCODES = Path("core/shader/recompiler/RdnaDecoder/include/RdnaDecoder/RdnaOpcode.hpp")


def commit(root, day, message, body):
    library = root / "core/libs/prx/libSceExample"
    library.mkdir(parents=True, exist_ok=True)
    (library / "Export.cpp").write_text(body)
    (root / OPCODES).parent.mkdir(parents=True, exist_ok=True)
    (root / OPCODES).write_text("enum class RdnaOpcode : int {\n    SEndpgm,\n};\n")
    env = {**os.environ, "GIT_AUTHOR_DATE": f"{day}T12:00:00Z", "GIT_COMMITTER_DATE": f"{day}T12:00:00Z"}
    subprocess.run(["git", "-C", str(root), "add", "-A"], check=True)
    subprocess.run(["git", "-C", str(root), "-c", "user.name=t", "-c", "user.email=t@t", "commit", "-q", "-m", message],
                   check=True, env=env)


class ProgressHistoryTests(unittest.TestCase):
    def test_one_point_per_day_at_the_last_commit(self):
        stub = "static void Stub() { NotImplemented_nid_no_patch(__func__); }\n"
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            subprocess.run(["git", "-C", directory, "init", "-q"], check=True)
            commit(root, "2026-01-01", "first", "int APS5_VABI A() { Stub(); return 0; }\n" + stub)
            commit(root, "2026-01-02", "second", "int APS5_VABI A() { return 0; }\nint APS5_VABI B() { return 0; }\n")
            commit(root, "2026-01-02", "third", "int APS5_VABI A() { return 0; }\nint APS5_VABI B() { Stub(); return 0; }\n" + stub)
            with patch.multiple(progress, ROOT=root, PRX=root / "core/libs/prx", OPCODES=root / OPCODES):
                points = progress_history.collect("HEAD")
                html = progress_history.page(points)
        self.assertEqual([p["date"] for p in points], ["2026-01-01", "2026-01-02"])
        self.assertEqual([(p["libraries"]["done"], p["libraries"]["total"]) for p in points], [(0, 1), (1, 2)])
        self.assertIn("<svg", html)
        self.assertIn("2026-01-02", html)

    def test_long_series_keeps_the_chart_readable(self):
        def series(count):
            start = date(2026, 1, 1).toordinal()
            return [{"date": date.fromordinal(start + i).isoformat(),
                     **{name: {"done": i, "total": count, "percent": 100 * i / count} for name in ("libraries", "shaders")}}
                    for i in range(count)]
        short, medium, long = (progress_history.chart(series(count)) for count in (20, 100, 400))
        week = progress_history.chart(series(10))
        self.assertEqual((week.count('class="x"'), week.count("<circle")), (10, 20))
        self.assertEqual((short.count('class="x"'), short.count("<circle")), (3, 40))
        self.assertEqual((medium.count('class="x"'), medium.count("<circle")), (14, 0))
        self.assertEqual((long.count('class="x"'), long.count("<circle")), (14, 0))

    def test_each_panel_shows_its_declared_total(self):
        point = {"date": "2026-01-01", "libraries": {"done": 3292, "total": 3706, "percent": 88.83},
                 "shaders": {"done": 1166, "total": 1166, "percent": 100.0}}
        svg = progress_history.chart([point, {**point, "date": "2026-01-02"}])
        self.assertIn("Declared 3,706", svg)
        self.assertIn("Declared 1,166", svg)
        self.assertIn("Implemented 1,166 (100%)", svg)


if __name__ == "__main__":
    unittest.main()
