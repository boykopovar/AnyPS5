import tempfile
import sys
import unittest
from pathlib import Path
from unittest.mock import patch
from urllib.error import URLError

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import nid_names


class DatabaseCacheTests(unittest.TestCase):
    def test_interrupted_download_does_not_publish_a_partial_cache(self):
        with tempfile.TemporaryDirectory() as directory:
            cache = Path(directory) / "cache" / "aerolib.csv"

            def interrupted(url, filename):
                Path(filename).write_text("nid1 scePartial\n", encoding="utf-8")
                raise URLError("connection interrupted")

            with patch.object(nid_names.urllib.request, "urlretrieve", side_effect=interrupted):
                with self.assertRaises(URLError):
                    nid_names.load_db(cache)
            self.assertFalse(cache.exists())
            self.assertEqual(list(cache.parent.iterdir()), [])

    def test_retry_after_interruption_downloads_the_complete_database(self):
        with tempfile.TemporaryDirectory() as directory:
            cache = Path(directory) / "aerolib.csv"

            def interrupted(url, filename):
                Path(filename).write_text("nid1 scePartial\n", encoding="utf-8")
                raise URLError("connection interrupted")

            def complete(url, filename):
                Path(filename).write_text("nid1 sceComplete\nnid2 sceOther\n", encoding="utf-8")

            with patch.object(nid_names.urllib.request, "urlretrieve", side_effect=interrupted):
                with self.assertRaises(URLError):
                    nid_names.load_db(cache)
            with patch.object(nid_names.urllib.request, "urlretrieve", side_effect=complete) as download:
                self.assertEqual(nid_names.load_db(cache), {"nid1": "sceComplete", "nid2": "sceOther"})
                download.assert_called_once()
            self.assertEqual(list(cache.parent.iterdir()), [cache])

    def test_cache_is_published_only_after_download_completes(self):
        with tempfile.TemporaryDirectory() as directory:
            cache = Path(directory) / "aerolib.csv"

            def complete(url, filename):
                self.assertEqual(url, nid_names.DB_URL)
                self.assertNotEqual(Path(filename), cache)
                self.assertEqual(Path(filename).parent, cache.parent)
                self.assertFalse(cache.exists())
                Path(filename).write_text("nid1 sceComplete\n", encoding="utf-8")

            with patch.object(nid_names.urllib.request, "urlretrieve", side_effect=complete):
                self.assertEqual(nid_names.load_db(cache), {"nid1": "sceComplete"})
            self.assertEqual(cache.read_text(encoding="utf-8"), "nid1 sceComplete\n")
            self.assertEqual(list(cache.parent.iterdir()), [cache])

    def test_existing_cache_is_used_without_downloading(self):
        with tempfile.TemporaryDirectory() as directory:
            cache = Path(directory) / "aerolib.csv"
            cache.write_text("nid1 sceExisting\n", encoding="utf-8")
            with patch.object(nid_names.urllib.request, "urlretrieve") as download:
                self.assertEqual(nid_names.load_db(cache), {"nid1": "sceExisting"})
                download.assert_not_called()


if __name__ == "__main__":
    unittest.main()
