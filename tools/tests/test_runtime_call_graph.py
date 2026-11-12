import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import runtime_call_graph


def edge(calls=1, consumer="game.guest.prx", consumer_offset="0x120", address="0x1120",
         library="libSceAudioOut", name="sceAudioOutOutput", provider="libSceAudioOut.guest.prx",
         provider_offset="0x80"):
    return {"calls": calls,
            "consumer": {"module": consumer, "offset": consumer_offset, "address": address},
            "import": {"library": library, "name": name},
            "provider": {"module": provider, "offset": provider_offset}}


def document(edges=None):
    return {"version": 1, "total_calls": 12, "dropped_callsite_events": 0,
            "edges": [edge(), edge(calls=4, consumer="engine.guest.prx", consumer_offset="0x50",
                                   address="0x1050", name="sceAudioOutInit"), *(edges or [])]}


class RuntimeCallGraphTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.path = Path(self.tmp.name) / "graph.json"

    def write(self, value):
        self.path.write_text(json.dumps(value), encoding="utf-8")
        return self.path

    def test_loader_aggregates_duplicate_edges_and_sorts_by_calls(self):
        graph = runtime_call_graph.load_graph(self.write(document([edge(calls=2)])))
        self.assertEqual(graph["total_calls"], 12)
        self.assertEqual([item["calls"] for item in graph["edges"]], [4, 3])
        self.assertEqual(graph["edges"][1]["import"]["name"], "sceAudioOutOutput")

    def test_unresolved_images_keep_the_runtime_address(self):
        item = edge(consumer=None, consumer_offset=None, address="0x9980", provider=None, provider_offset=None)
        graph = runtime_call_graph.load_graph(self.write(document([item])))
        unresolved = next(record for record in graph["edges"] if record["consumer"]["address"] == "0x9980")
        self.assertEqual(unresolved["consumer"], {"module": None, "offset": None, "address": "0x9980"})
        self.assertIsNone(unresolved["provider"]["module"])

    def test_report_filters_and_limits_edges(self):
        graph = runtime_call_graph.load_graph(self.write(document([edge(library="libSceNet")])))
        text = runtime_call_graph.report(graph, limit=1, library="libSceAudioOut")
        self.assertIn("4 calls across 1 shown edges", text)
        self.assertIn("sceAudioOutInit", text)
        self.assertNotIn("sceAudioOutOutput", text)

    def test_markdown_cells_escape_pipes_and_newlines(self):
        graph = runtime_call_graph.load_graph(self.write(document([edge(name="line|one\ntwo")])))
        text = runtime_call_graph.report(graph, library="libSceAudioOut")
        self.assertIn(r"line\|one two", text)

    def test_rejects_unsupported_version(self):
        value = document()
        value["version"] = 2
        with self.assertRaisesRegex(ValueError, "unsupported graph format"):
            runtime_call_graph.load_graph(self.write(value))

    def test_rejects_non_integer_version(self):
        value = document()
        value["version"] = 1.0
        with self.assertRaisesRegex(ValueError, "unsupported graph format"):
            runtime_call_graph.load_graph(self.write(value))

    def test_rejects_boolean_counts(self):
        value = document()
        value["total_calls"] = True
        with self.assertRaisesRegex(ValueError, "non-negative integer"):
            runtime_call_graph.load_graph(self.write(value))

    def test_rejects_zero_call_edges(self):
        with self.assertRaisesRegex(ValueError, "positive count"):
            runtime_call_graph.load_graph(self.write(document([edge(calls=0)])))

    def test_rejects_unpaired_module_and_offset(self):
        item = edge(consumer=None, consumer_offset="0x123")
        with self.assertRaisesRegex(ValueError, "both be resolved or both be null"):
            runtime_call_graph.load_graph(self.write(document([item])))

    def test_rejects_malformed_offsets(self):
        item = edge(provider_offset="0X10")
        with self.assertRaisesRegex(ValueError, "lowercase hexadecimal"):
            runtime_call_graph.load_graph(self.write(document([item])))

    def test_cli_writes_markdown_and_json_and_rejects_missing_files(self):
        self.write(document())
        tool = Path(runtime_call_graph.__file__)
        done = subprocess.run([sys.executable, str(tool), str(self.path)], capture_output=True, text=True)
        self.assertEqual(done.returncode, 0, done.stderr)
        self.assertIn("# Runtime import provider graph", done.stdout)
        machine = subprocess.run([sys.executable, str(tool), str(self.path), "--json"], capture_output=True, text=True)
        self.assertEqual(machine.returncode, 0, machine.stderr)
        self.assertEqual(json.loads(machine.stdout)["version"], 1)
        missing = subprocess.run([sys.executable, str(tool), str(self.path / "missing")], capture_output=True, text=True)
        self.assertEqual(missing.returncode, 2)
        self.assertIn("cannot read graph", missing.stderr)


if __name__ == "__main__":
    unittest.main()
