import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import progress

WORKFLOW = ROOT / ".github/workflows/progress-report.yml"


def pull_request_paths(text):
    head = text.split("permissions:", 1)[0]
    return [line.strip()[3:-1] for line in head.splitlines() if line.strip().startswith("- '")]


def matches(path, filters):
    return any(path == item or (item.endswith("/**") and path.startswith(item[:-2])) for item in filters)


class ProgressReportInputTests(unittest.TestCase):
    def inputs(self):
        script = Path(progress.__file__).resolve()
        for path in (script, progress.OPCODES, progress.ISA, script.with_name("nid_names.py")):
            yield path.resolve().relative_to(ROOT).as_posix()

    def test_every_file_the_report_reads_starts_it(self):
        filters = pull_request_paths(WORKFLOW.read_text(encoding="utf-8"))
        missing = [path for path in self.inputs() if not matches(path, filters)]
        self.assertEqual(missing, [])

    def test_library_sources_start_it(self):
        filters = pull_request_paths(WORKFLOW.read_text(encoding="utf-8"))
        self.assertTrue(matches("core/libs/prx/libSceVideoOut/src/VideoOut.cpp", filters))


if __name__ == "__main__":
    unittest.main()
