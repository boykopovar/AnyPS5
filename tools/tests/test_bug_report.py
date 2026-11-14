import sys
from pathlib import Path
import unittest

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


if __name__ == "__main__":
    unittest.main()
