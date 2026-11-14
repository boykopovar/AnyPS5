import sys
import tempfile
from pathlib import Path
import unittest
import zipfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import bug_report


class RedactTests(unittest.TestCase):
    def test_home_user_host_and_ipv4_are_replaced(self):
        text = "opened /home/ana/games/title.elf on serverfestivo from 192.168.1.20"
        out = bug_report.redact(text, home="/home/ana", user="ana", host="serverfestivo")
        self.assertEqual(out, "opened ~/games/title.elf on <host> from <ip>")

    def test_home_is_replaced_before_user(self):
        out = bug_report.redact("C:\\Users\\ana\\AppData", home="C:\\Users\\ana", user="ana", host="x1")
        self.assertEqual(out, "~\\AppData")

    def test_user_is_matched_as_a_whole_word(self):
        out = bug_report.redact("banana ana ana.txt ana-dir", home="", user="ana", host="")
        self.assertEqual(out, "banana <user> <user>.txt ana-dir")

    def test_short_or_empty_identities_are_not_replaced(self):
        text = "a b /x"
        self.assertEqual(bug_report.redact(text, home="/x", user="a", host="b"), "a b /x")

    def test_text_without_matches_is_unchanged(self):
        text = "frames=120 averageFps=59.9 version 1.2.3"
        self.assertEqual(bug_report.redact(text, home="/home/ana", user="ana", host="box"), "frames=120 averageFps=59.9 version 1.2.3")


class BundleTests(unittest.TestCase):
    identity = {"home": "/home/ana", "user": "ana", "host": "serverfestivo"}

    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.root = Path(self.directory.name)
        self.report = self.root / "anyps5-report.json"
        self.report.write_text('{"game": {"name": "title"}, "run": {"path": "/home/ana/games/title.elf", "from": "10.0.0.7"}}', encoding="utf-8")
        self.log = self.root / "run.log"
        self.log.write_text("[sce] opened /home/ana/games/title.elf\nserverfestivo: 192.168.1.20 connected\n", encoding="utf-8")
        self.ini = self.root / "anyps5-input.ini"
        self.ini.write_text("Cross = KEY:F\n", encoding="utf-8")

    def tearDown(self):
        self.directory.cleanup()

    def bundle(self, **kwargs):
        out = self.root / "bundle.zip"
        names = bug_report.make_bundle(self.report, out, log=kwargs.pop("log", self.log), input_config=kwargs.pop("input_config", self.ini), **self.identity)
        return out, names

    def test_members_are_redacted_and_no_identity_text_remains(self):
        out, names = self.bundle()
        self.assertEqual(names, ["README.txt", "anyps5-input.ini", "anyps5-report.json", "anyps5.log", "host.txt"])
        with zipfile.ZipFile(out) as bundle:
            contents = {name: bundle.read(name).decode("utf-8") for name in bundle.namelist()}
        self.assertIn('"path": "~/games/title.elf"', contents["anyps5-report.json"])
        self.assertIn('"from": "<ip>"', contents["anyps5-report.json"])
        self.assertIn("[sce] opened ~/games/title.elf", contents["anyps5.log"])
        self.assertIn("<host>: <ip> connected", contents["anyps5.log"])
        for name, text in contents.items():
            for secret in ("/home/ana", "serverfestivo", "192.168.1.20", "10.0.0.7"):
                self.assertNotIn(secret, text, f"{secret} leaked into {name}")

    def test_missing_log_and_input_are_left_out(self):
        _, names = self.bundle(log=self.root / "absent.log", input_config=None)
        self.assertEqual(names, ["README.txt", "anyps5-report.json", "host.txt"])

    def test_report_that_is_not_json_is_refused(self):
        self.report.write_text("not json", encoding="utf-8")
        with self.assertRaises(ValueError):
            self.bundle()
        self.assertFalse((self.root / "bundle.zip").exists())

    def test_existing_bundle_is_not_overwritten(self):
        (self.root / "bundle.zip").write_bytes(b"keep")
        with self.assertRaises(FileExistsError):
            self.bundle()
        self.assertEqual((self.root / "bundle.zip").read_bytes(), b"keep")


if __name__ == "__main__":
    unittest.main()
