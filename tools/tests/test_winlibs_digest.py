import hashlib
import os
import re
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

REPOSITORY = Path(__file__).resolve().parents[2]
WORKFLOWS = (".github/workflows/build.yml", ".github/workflows/release.yml")
DIGEST = "cb2fbad6162540cdf5e1facdce08d4dac359e8cf64f7f696a99274291763b815"
DOWNLOAD = re.compile(r'^\s*curl .*winlibs_mingw/releases/download/.*\$WINLIBS_ZIP"\s*$')
CHECK = '          echo "$WINLIBS_SHA256  $RUNNER_TEMP/winlibs.zip" | sha256sum -c -'


class WinlibsDigestTests(unittest.TestCase):
    def workflow(self, name):
        return (REPOSITORY / name).read_text(encoding="utf-8").splitlines()

    def test_every_winlibs_download_is_followed_by_the_digest_check(self):
        for name in WORKFLOWS:
            lines = self.workflow(name)
            downloads = [index for index, line in enumerate(lines) if DOWNLOAD.match(line)]
            self.assertTrue(downloads, name)
            for index in downloads:
                with self.subTest(workflow=name, line=index + 1):
                    self.assertEqual(lines[index + 1], CHECK)

    def test_the_pinned_digest_is_defined_next_to_the_archive_name(self):
        for name in WORKFLOWS:
            text = "\n".join(self.workflow(name))
            with self.subTest(workflow=name):
                self.assertEqual(text.count("WINLIBS_SHA256: " + DIGEST), text.count("WINLIBS_ZIP: "))
                self.assertGreater(text.count("WINLIBS_ZIP: "), 0)

    @unittest.skipUnless(sys.platform != "win32" and shutil.which("bash") and shutil.which("sha256sum"), "runs on POSIX where bash is the one that runs the workflow steps")
    def test_the_check_accepts_the_matching_archive_and_rejects_another(self):
        command = CHECK.strip()
        with tempfile.TemporaryDirectory() as directory:
            archive = Path(directory, "winlibs.zip")
            archive.write_bytes(b"not the compiler archive")
            environment = dict(os.environ, RUNNER_TEMP=directory)
            environment["WINLIBS_SHA256"] = "0" * 64
            rejected = subprocess.run(["bash", "-e", "-o", "pipefail", "-c", command], env=environment, capture_output=True)
            self.assertNotEqual(rejected.returncode, 0)
            environment["WINLIBS_SHA256"] = hashlib.sha256(archive.read_bytes()).hexdigest()
            accepted = subprocess.run(["bash", "-e", "-o", "pipefail", "-c", command], env=environment, capture_output=True)
            self.assertEqual(accepted.returncode, 0, accepted.stderr)


if __name__ == "__main__":
    unittest.main()
