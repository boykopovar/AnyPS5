import re
import sys
import unittest
from pathlib import Path

DRIVER = Path(__file__).resolve().parents[1]
COMMAND_LIST = DRIVER / "Graphics/include/CommandList.hpp"

LITERAL = re.compile(r'"vk(Cmd\w+)"')
NAMED_RESOLVE = re.compile(r'(?:deviceProc|vkGetDeviceProcAddr|GetDeviceProcAddr)\s*\([^;]*?"vkCmd\w+"')
GENERIC_RESOLVE = re.compile(r'\b(?:deviceProc|resolve)\s*\(\s*\w+\s*,\s*(?![\s"])[^;)]*\)')
THUNKED = re.compile(r'CommandThunk\s*\(')

RESOLVERS = {
    "Graphics/include/Context.hpp": "Context::lookup, which returns a thunk for every listed command",
    "Execution/src/VulkanDevice.cpp": "VulkanDevice's DeviceFunction, which returns a thunk for every listed command",
    "Graphics/src/CommandThunks.cpp": "ResolveCommandFunctions, the real entry points that only replay and demotion call",
}


def listed_commands(text):
    return set(re.findall(r'X\((Cmd\w+)\)', text))


def unlisted(text, listed):
    return sorted({name for name in LITERAL.findall(text) if name not in listed})


def unthunked_named(text):
    return [number for number, line in enumerate(text.splitlines(), 1) if NAMED_RESOLVE.search(line) and not THUNKED.search(line)]


def generic_sites(text):
    return [number for number, line in enumerate(text.splitlines(), 1) if GENERIC_RESOLVE.search(line)]


def sources(root):
    return sorted(list((root / "Graphics").rglob("*.[ch]pp")) + list((root / "Execution").rglob("*.[ch]pp")))


def problems(root, listed, resolvers):
    messages = []
    for path in sources(root):
        name = path.relative_to(root).as_posix()
        text = path.read_text(encoding="utf-8", errors="replace")
        missing = unlisted(text, listed)
        if missing:
            messages.append(f"{name}: {', '.join(missing)} not in APS5_COMMANDS: a stream handle passed to it would reach the driver; add it to Graphics/include/CommandList.hpp")
        raw = unthunked_named(text)
        if raw:
            messages.append(f"{name}: a vkCmd entry point resolved without CommandThunk (lines {', '.join(map(str, raw))}): resolve it through Context::Function or wrap it in CommandThunk")
        generic = generic_sites(text)
        if generic and name not in resolvers:
            messages.append(f"{name}: resolves device functions by a variable name (lines {', '.join(map(str, generic))}): use Context::Function, or wrap vkCmd names in CommandThunk and add the file to RESOLVERS")
    for name in sorted(resolvers):
        path = root / name
        if not path.exists():
            messages.append(f"{name}: listed in RESOLVERS but missing")
        elif not THUNKED.search(path.read_text(encoding="utf-8", errors="replace")):
            messages.append(f"{name}: a resolver that never calls CommandThunk")
    return messages


class RulesTest(unittest.TestCase):
    def test_a_literal_outside_the_list_is_reported(self):
        self.assertEqual(unlisted('f<PFN_vkCmdDraw>("vkCmdDraw"); f("vkCmdTraceRaysKHR");', {"CmdDraw"}), ["CmdTraceRaysKHR"])

    def test_a_named_resolve_needs_a_thunk(self):
        text = "\n".join([
            'auto a = reinterpret_cast<PFN_vkCmdDraw>(context.deviceProc(context.device, "vkCmdDraw"));',
            'auto b = reinterpret_cast<PFN_vkCmdDraw>(CommandThunk("vkCmdDraw", context.deviceProc(context.device, "vkCmdDraw")));',
            'auto c = reinterpret_cast<PFN_vkDestroyBuffer>(context.deviceProc(context.device, "vkDestroyBuffer"));',
        ])
        self.assertEqual(unthunked_named(text), [1])

    def test_a_generic_resolve_is_found(self):
        self.assertEqual(generic_sites("const auto function = deviceProc(device, name);\nint x = 0;"), [1])
        self.assertEqual(generic_sites('auto f = deviceProc(device, "vkDestroyFence");'), [])

    def test_a_tree_with_a_raw_resolver_fails(self):
        import tempfile
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "Graphics").mkdir()
            (root / "Graphics/Raw.cpp").write_text('auto f = deviceProc(device, name);\nauto g = context.deviceProc(context.device, "vkCmdDraw");\nh("vkCmdTraceRaysKHR");\n')
            messages = problems(root, {"CmdDraw"}, {})
            self.assertEqual(len(messages), 3, messages)

    def test_the_list_parses(self):
        self.assertIn("CmdDraw", listed_commands(COMMAND_LIST.read_text(encoding="utf-8")))


class DriverTreeTest(unittest.TestCase):
    def test_every_command_resolves_through_a_thunk(self):
        listed = listed_commands(COMMAND_LIST.read_text(encoding="utf-8"))
        messages = problems(DRIVER, listed, RESOLVERS)
        self.assertEqual(messages, [], "\n".join(messages))


if __name__ == "__main__":
    unittest.main(argv=sys.argv[:1])
