import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import build_mingw_host_only_tests


class BuildMingwHostOnlyTestsTests(unittest.TestCase):
    def test_link_command_joins_fragments_and_tolerates_missing_link(self):
        target = {"link": {"commandFragments": [{"fragment": "a.o"}, {"fragment": "-lm"}]}}
        self.assertEqual(build_mingw_host_only_tests.link_command(target), "a.o -lm")
        self.assertEqual(build_mingw_host_only_tests.link_command({}), "")


if __name__ == "__main__":
    unittest.main()
