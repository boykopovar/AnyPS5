import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

CHECKER = Path(__file__).resolve().parents[1] / "check_conventions.py"


class DocumentationLinksTests(unittest.TestCase):
    def test_external_links_do_not_hide_missing_repository_links(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)

            def git(*args):
                return subprocess.run(
                    ["git", *args], cwd=root, capture_output=True,
                    encoding="utf-8", check=True,
                )

            git("init")
            git("config", "user.name", "Test Author")
            git("config", "user.email", "test@example.com")
            (root / "README.md").write_text("Project\n", encoding="utf-8")
            docs = root / "docs" / "dev"
            docs.mkdir(parents=True)
            document = docs / "LINKS.md"
            document.write_text("Links\n", encoding="utf-8")
            git("add", ".")
            git("commit", "-m", "docs: add link fixture")

            document.write_text(
                "[https](https://example.com)\n"
                "[uppercase](HTTPS://example.com)\n"
                "[mixed case](HtTpS://example.com)\n"
                "[compound scheme](git+https://example.com/repo)\n"
                "[scheme punctuation](demo.v1-test:value)\n"
                "[email](mailto:test@example.com)\n"
                "[network path](//example.com/reference)\n"
                "[fragment](#section)\n"
                "[relative](../../README.md)\n"
                "[root](/README.md)\n"
                "[directory](/docs/dev)\n"
                "[fragment and query](../../README.md?raw=1#section)\n"
                "[missing](missing.md)\n",
                encoding="utf-8",
            )
            git("add", ".")
            git("commit", "-m", "docs: exercise link validation")
            environment = os.environ.copy()
            for name in ("GITHUB_ACTIONS", "GITHUB_EVENT_PATH", "GITHUB_STEP_SUMMARY"):
                environment.pop(name, None)
            result = subprocess.run(
                [sys.executable, str(CHECKER), "--base", "HEAD^"],
                cwd=root, capture_output=True, encoding="utf-8", timeout=10,
                env=environment,
            )
            self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
            self.assertEqual(result.stderr, "")
            self.assertEqual(
                result.stdout.splitlines(),
                [
                    "docs/dev/LINKS.md:13: error: [doc-link] "
                    "Relative link to a file that does not exist: missing.md",
                    "1 findings, 1 errors; GitHub shows at most 10 error annotations, "
                    "the job summary lists all",
                ],
            )


if __name__ == "__main__":
    unittest.main()
