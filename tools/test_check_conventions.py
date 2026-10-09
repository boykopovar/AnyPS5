
import unittest

from tools.check_conventions import links


class TestMarkdownLinks(unittest.TestCase):

    def test_encoded_space(self):
        result = links(
            "docs/README.md",
            "[Guide](Build%20Guide.md)"
        )
        self.assertEqual(
            result,
            [("Build%20Guide.md", "docs/Build Guide.md")]
        )

    def test_encoded_unicode(self):
        result = links(
            "docs/README.md",
            "[Guide](caf%C3%A9.md)"
        )
        self.assertEqual(
            result,
            [("caf%C3%A9.md", "docs/café.md")]
        )

    def test_normal_link(self):
        result = links(
            "docs/README.md",
            "[Guide](Guide.md)"
        )
        self.assertEqual(
            result,
            [("Guide.md", "docs/Guide.md")]
        )

    def test_root_relative_link(self):
        result = links(
            "docs/README.md",
            "[Guide](/docs/Build%20Guide.md)"
        )
        self.assertEqual(
            result,
            [("/docs/Build%20Guide.md", "docs/Build Guide.md")]
        )

    def test_external_link(self):
        result = links(
            "README.md",
            "[Google](https://google.com)"
        )
        self.assertEqual(result, [])

    def test_anchor_link(self):
        result = links(
            "README.md",
            "[Section](#installation)"
        )
        self.assertEqual(result, [])


if __name__ == "__main__":
    unittest.main()
