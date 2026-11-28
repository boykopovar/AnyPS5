import ast
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

TOOLS = Path(__file__).resolve().parents[1]
SUBJECT = "João"
CALLS = {"run", "check_output", "check_call", "call", "Popen"}


def decodes_with_locale(call):
    target = call.func
    if not isinstance(target, ast.Attribute) or target.attr not in CALLS:
        return False
    if not isinstance(target.value, ast.Name) or target.value.id != "subprocess":
        return False
    names = {keyword.arg for keyword in call.keywords}
    textual = any(keyword.arg in ("text", "universal_newlines") and getattr(keyword.value, "value", None) is True
                  for keyword in call.keywords)
    return textual and "encoding" not in names


def sources():
    for path in sorted(TOOLS.rglob("*.py")):
        if "tests" not in path.relative_to(TOOLS).parts and not path.name.startswith("test_"):
            yield path


class SubprocessEncodingTests(unittest.TestCase):
    def test_tools_name_the_encoding_of_subprocess_text(self):
        found = []
        for path in sources():
            tree = ast.parse(path.read_text(encoding="utf-8"), filename=str(path))
            found += [f"{path.relative_to(TOOLS).as_posix()}:{node.lineno}" for node in ast.walk(tree)
                      if isinstance(node, ast.Call) and decodes_with_locale(node)]
        self.assertEqual(found, [])

    def test_git_helpers_read_utf8_under_a_legacy_locale(self):
        with tempfile.TemporaryDirectory() as directory:
            identity = ("-c", "user.name=fixture", "-c", "user.email=fixture@example.com")
            subprocess.run(["git", "-C", directory, "init", "-q"], check=True)
            subprocess.run(["git", "-C", directory, *identity, "commit", "-q", "--allow-empty", "-F", "-"],
                           input=SUBJECT.encode("utf-8"), check=True)
            environment = {**os.environ, "LC_ALL": "C", "PYTHONUTF8": "0", "PYTHONCOERCECLOCALE": "0"}
            for module in ("title_run", "vulkan_validation_gate"):
                code = (f"import sys; sys.path.insert(0, sys.argv[1]); import {module} as tool; "
                        "print(ascii(tool.git(sys.argv[2], 'log', '-1', '--format=%s')))")
                with self.subTest(module=module):
                    result = subprocess.run([sys.executable, "-c", code, str(TOOLS), directory], capture_output=True,
                                            text=True, encoding="utf-8", errors="replace", env=environment, timeout=20)
                    self.assertEqual(result.returncode, 0, result.stderr)
                    self.assertEqual(result.stdout.strip(), ascii(SUBJECT))


if __name__ == "__main__":
    unittest.main()
