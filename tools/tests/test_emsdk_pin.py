import re
import unittest
from pathlib import Path

REPOSITORY = Path(__file__).resolve().parents[2]
WORKFLOWS = (".github/workflows/release.yml", ".github/workflows/emscripten-libraries.yml")
CLONE = re.compile(r"git clone --depth 1 --branch (\S+) https://github.com/emscripten-core/emsdk\.git")
INSTALL = re.compile(r'"\$RUNNER_TEMP/emsdk/emsdk" (?:install|activate) (\S+)')


class EmsdkPinTests(unittest.TestCase):
    def workflow(self, name):
        return (REPOSITORY / name).read_text(encoding="utf-8")

    def clone_version(self, text):
        match = CLONE.search(text)
        self.assertIsNotNone(match)
        return match.group(1)

    def test_every_emsdk_clone_is_a_release_tag(self):
        for name in WORKFLOWS:
            with self.subTest(workflow=name):
                self.assertEqual(len(CLONE.findall(self.workflow(name))), 1)

    def test_each_job_installs_and_activates_the_version_it_clones(self):
        for name in WORKFLOWS:
            text = self.workflow(name)
            clone = self.clone_version(text)
            versions = INSTALL.findall(text)
            with self.subTest(workflow=name):
                self.assertEqual(versions, [clone, clone])

    def test_the_release_and_the_portability_job_use_the_same_version(self):
        clones = {name: self.clone_version(self.workflow(name)) for name in WORKFLOWS}
        self.assertEqual(len(set(clones.values())), 1, clones)


if __name__ == "__main__":
    unittest.main()
