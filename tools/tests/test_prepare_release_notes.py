import os
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import prepare_release_notes


class OutputPathTests(unittest.TestCase):
    def test_resolve_output_path_returns_a_regular_file_resolved(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'notes.md'
            path.write_text('x', encoding='utf-8')
            self.assertEqual(prepare_release_notes.resolve_output_path(path), path.resolve())


if __name__ == "__main__":
    unittest.main()
