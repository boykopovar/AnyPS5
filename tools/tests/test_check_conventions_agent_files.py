import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from check_conventions import allowed_notes


class AgentFileTests(unittest.TestCase):
    def test_agent_files_are_rejected_in_any_case(self):
        for name in (
            "CLAUDE.md", "claude.md", "Claude.md", "GEMINI.md",
            "Copilot-Instructions.md", ".CursorRules", ".WindsurfRules", "windsurf.md",
            "ChatGPT.md", ".ChatGPTRules", ".aider.conf.yml",
        ):
            with self.subTest(name=name):
                self.assertIs(allowed_notes(f"docs/dev/{name}"), False)
                self.assertIs(allowed_notes(name), False)

    def test_only_the_root_agents_file_is_allowed(self):
        self.assertIs(allowed_notes("AGENTS.md"), True)
        for path in ("agents.md", "Agents.md", "docs/dev/AGENTS.md", "core/AGENTS.md", "tools/agents.md"):
            with self.subTest(path=path):
                self.assertIs(allowed_notes(path), False)

    def test_ordinary_docs_are_still_allowed(self):
        self.assertTrue(allowed_notes("docs/dev/Claudette.md"))
        self.assertTrue(allowed_notes("README.md"))
        self.assertIs(allowed_notes("core/x.cpp"), None)


if __name__ == "__main__":
    unittest.main()
