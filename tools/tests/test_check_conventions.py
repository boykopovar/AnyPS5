import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import check_conventions

NEXT = "int APS5_VABI sceNext() { return 1; }\n"


def body(source):
    return check_conventions.function_body(source.splitlines(), 0)


def is_stub(source):
    result = body(source)
    return result is not None and check_conventions.STUB_BODY.match(result) is not None


class FunctionBodyTests(unittest.TestCase):
    def test_plain_body(self):
        self.assertEqual(body("int APS5_VABI sceFoo() {\n    return 0;\n}\n" + NEXT), "return0;")

    def test_line_comment_on_its_own_line(self):
        source = "int APS5_VABI sceFoo() {\n    // nothing to do\n    return 0;\n}\n" + NEXT
        self.assertEqual(body(source), "return0;")

    def test_trailing_line_comment(self):
        source = "int APS5_VABI sceFoo() {\n    return 0;  // always succeeds\n}\n" + NEXT
        self.assertEqual(body(source), "return0;")

    def test_line_comment_before_the_closing_brace(self):
        source = "int APS5_VABI sceFoo() {\n    return 0;\n    // end\n}\n" + NEXT
        self.assertEqual(body(source), "return0;")

    def test_block_comment(self):
        source = "int APS5_VABI sceFoo() {\n    /* nothing to do */\n    return 0;\n}\n" + NEXT
        self.assertEqual(body(source), "return0;")

    def test_comment_markers_inside_strings_are_not_comments(self):
        source = 'int APS5_VABI sceFoo() {\n    puts("http://example.com");\n    return 0;\n}\n' + NEXT
        self.assertEqual(body(source), 'puts("");return0;')

    def test_prototype_has_no_body(self):
        self.assertIsNone(body("int APS5_VABI sceFoo();\n" + NEXT))


class StubDetectionTests(unittest.TestCase):
    def test_empty_return_is_a_stub(self):
        self.assertTrue(is_stub("int APS5_VABI sceFoo() {\n    return 0;\n}\n" + NEXT))

    def test_stub_with_line_comment_is_still_a_stub(self):
        source = "int APS5_VABI sceFoo() {\n    // nothing to do\n    return 0;\n}\n" + NEXT
        self.assertTrue(is_stub(source))

    def test_stub_with_trailing_line_comment_is_still_a_stub(self):
        source = "int APS5_VABI sceFoo() {\n    return 0;  // always succeeds\n}\n" + NEXT
        self.assertTrue(is_stub(source))

    def test_function_with_logic_is_not_a_stub(self):
        source = "int APS5_VABI sceFoo(int a) {\n    // doubles\n    return a * 2;\n}\n" + NEXT
        self.assertFalse(is_stub(source))


if __name__ == "__main__":
    unittest.main()