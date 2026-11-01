import importlib.util
import json
import re
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

TOOLS = Path(__file__).resolve().parent.parent


def load(name, file):
    spec = importlib.util.spec_from_file_location(name, TOOLS / file)
    module = importlib.util.module_from_spec(spec)
    sys.modules[name] = module
    spec.loader.exec_module(module)
    return module


shader_scan = load("anyps5_shader_scan", "shader_scan.py")
progress = load("anyps5_progress_for_shader_scan", "progress.py")

DECODER = "core/shader/recompiler/RdnaDecoder/src/"
KEYS = {"VOP3:VOP1": "Vop3EncodedVop1", "MIMG": "Image", "VOP3P": "Vop3p"}
ENTRIES = {"SOP1": {3: "SMovB32"}, "VOP2": {0: "VFirstF32", 3: "VAddF32", 5: "VMacF32", 0x3F: "VLastF32", 0x40: "VPastF32"}, "VOP1": {5: "VCvtF32I32"},
           "VOP3:VOP1": {0: "VMovB32", 2: "VCvtF32I32"}, "VOPC": {0xC0: "VCmpEqF32", 0x100: "VCmpNew"}}

ISA_XML = """<Spec><ISA><Instructions>
<Instruction><InstructionName>S_MOV_B32</InstructionName><InstructionEncodings>
  <InstructionEncoding><EncodingName>ENC_SOP1</EncodingName><Opcode Radix="10">3</Opcode></InstructionEncoding>
</InstructionEncodings></Instruction>
<Instruction><InstructionName>V_ADD_F32</InstructionName><InstructionEncodings>
  <InstructionEncoding><EncodingName>ENC_VOP2</EncodingName><Opcode Radix="10">3</Opcode></InstructionEncoding>
  <InstructionEncoding><EncodingName>ENC_VOP3</EncodingName><Opcode Radix="10">259</Opcode></InstructionEncoding>
  <InstructionEncoding><EncodingName>VOP2_VOP_DPP16</EncodingName><Opcode Radix="10">3</Opcode></InstructionEncoding>
  <InstructionEncoding><EncodingName>VOP2_VOP_DPP8</EncodingName><Opcode Radix="10">3</Opcode></InstructionEncoding>
  <InstructionEncoding><EncodingName>VOP2_VOP_SDWA</EncodingName><Opcode Radix="10">3</Opcode></InstructionEncoding>
</InstructionEncodings></Instruction>
<Instruction><InstructionName>GLOBAL_LOAD_DWORD</InstructionName><InstructionEncodings>
  <InstructionEncoding><EncodingName>ENC_FLAT_GLBL</EncodingName><Opcode Radix="10">20</Opcode></InstructionEncoding>
</InstructionEncodings></Instruction>
</Instructions></ISA></Spec>"""

ISA = [
    ("S_MOV_B32", "SOP1", "base", 3),
    ("S_NEW", "SOP1", "base", 9),
    ("V_ADD_F32", "VOP2", "base", 3),
    ("V_ADD_F32", "VOP2", "vop3", 0x103),
    ("V_MAC_F32", "VOP2", "vop3", 0x105),
    ("V_FIRST_F32", "VOP2", "vop3", 0x100),
    ("V_LAST_F32", "VOP2", "vop3", 0x13F),
    ("V_PAST_F32", "VOP2", "vop3", 0x140),
    ("V_ADD_F32", "VOP2", "dpp16", 3),
    ("V_ADD_F32", "VOP2", "dpp8", 3),
    ("V_CVT_F32_I32", "VOP1", "base", 5),
    ("V_CVT_F32_I32", "VOP1", "vop3", 0x182),
    ("V_CVT_F32_I32", "VOP1", "dpp16", 5),
    ("V_MOV_B32", "VOP1", "vop3", 0x180),
    ("V_CMP_EQ_F32", "VOPC", "vop3", 0xC0),
    ("V_CMP_NEW", "VOPC", "vop3", 0x100),
    ("EXP", "EXP", "base", 0),
]


def table_source(encoding):
    key = KEYS.get(encoding, encoding.capitalize())
    cases = "".join(f"        case 0x{number:02x}: return RdnaOpcode::{name};\n"
                    for number, name in ENTRIES.get(encoding, {0: "Placeholder" + key}).items())
    return f"RdnaOpcode lookup{key}Opcode(uint32_t op)\n{{\n    switch (op)\n    {{\n{cases}    }}\n}}\n\n"


def write_tree(root, tables=None, throws=True):
    root = Path(root)
    files = {
        f"{DECODER}Tables.cpp": "".join(table_source(e) for e in (tables or shader_scan.REQUIRED_TABLES)),
        f"{DECODER}Rules.cpp": (
            "bool isUnsupportedVop3EncodedVop2Alias(uint32_t op) { return op == 0x05; }\n"
            "bool isVop2LiteralMadOpcode(uint32_t op) { return op == 0x06; }\n"
            "void dispatch(RdnaInstructionFamily f) { switch (f) { case RdnaInstructionFamily::EXP: return; default: return; } }\n"
            "bool hasDppForm(RdnaOpcode op) { switch (op) { case RdnaOpcode::VCvtF32I32: return false; default: return true; } }\n"
            "bool acceptsDpp8(RdnaOpcode op) { switch (op) { case RdnaOpcode::VAddF32: return false; default: return true; } }\n"),
        "core/shader/recompiler/Translation/Translate.cpp": "".join(
            f"void TranslationContext::{emit}(const Instruction& i)\n{{\n    switch (i.op)\n    {{\n{cases}    }}\n}}\n"
            for emit, cases in (("emitScalar", "        case RdnaOpcode::SMovB32: break;\n"),
                                ("emitVector", "        case RdnaOpcode::VCmpEqF32: break;\n"), ("emitMemory", ""),
                                ("emitInterpolation", "        case RdnaOpcode::VCvtF32I32: break;\n")))
        + "void TranslateInstruction(const Instruction& i)\n{\n    switch (i.family)\n    {\n        case RdnaInstructionFamily::EXP: break;\n    }\n}\n",
    }
    if throws:
        files["core/shader/recompiler/Ir/Lower.cpp"] = (
            'void a() { throw std::runtime_error("Unsupported image type"); }\n'
            'void b() { throw std::runtime_error("bad size"); }\n'
            'void c() { try { f(); } catch (...) { throw; } }\n'
            '// throw std::runtime_error("unsupported in a comment");\n'
            '/* throw std::runtime_error("not implemented in a block comment"); */\n'
            'void d() { throw UnsupportedInstructionError(op); }\n'
            'void e() { throw std::runtime_error("semi; colon is not implemented"); }\n')
        files["core/shader/recompiler/Top.cpp"] = 'void f() { throw std::runtime_error("not implemented yet"); }\n'
        files["core/shader/recompiler/Ir/tests/LowerTests.cpp"] = 'void t() { throw std::runtime_error("Unsupported"); }\n'
    for path, text in files.items():
        (root / path).parent.mkdir(parents=True, exist_ok=True)
        (root / path).write_text(text, encoding="utf-8")
    formats = {
        "core/libs/prx/libSceAgcDriver/Graphics/src/Formats.cpp": (
            "enum class IrBufferFormat : std::uint32_t { Invalid = 0, R8 = 1, R16 = 2, R32 = 3, Rg8 = 4 };\n"
            "constexpr std::array DefinedImageFormats = { std::pair{1u, 4u}, std::pair{7u, 7u} };\n"
            "constexpr FormatInfo kFormatInfoTable[] = { {IrBufferFormat::R8, true}, {IrBufferFormat::R16, false}, {IrBufferFormat::R32, true} };\n"
            "FormatInfo GetFormatInfo(IrBufferFormat f)\n{\n    switch (f)\n    {\n        case IrBufferFormat::R16: return {};\n        case IrBufferFormat::R32: return {};\n    }\n}\n"),
        "core/libs/prx/libSceAgcDriver/Graphics/src/TextureFormat.cpp": (
            "constexpr Entry kFormatLookup[] = { {1, VK_FORMAT_R8_UNORM}, {3, VK_FORMAT_R32_UINT}, {9, VK_FORMAT_R8_UINT} };\n"
            "std::uint32_t remapGuestFormat(std::uint32_t f) { return f == 4 || f == 7 ? 3 : f; }\n"),
    }
    for path, text in formats.items():
        (root / path).parent.mkdir(parents=True, exist_ok=True)
        (root / path).write_text(text, encoding="utf-8")


class IsaTests(unittest.TestCase):
    def test_xml_forms_are_classified_and_sdwa_is_left_out(self):
        self.assertEqual(shader_scan.parse_isa_xml(ISA_XML), [
            ("S_MOV_B32", "SOP1", "base", 3),
            ("V_ADD_F32", "VOP2", "base", 3),
            ("V_ADD_F32", "VOP2", "vop3", 259),
            ("V_ADD_F32", "VOP2", "dpp16", 3),
            ("V_ADD_F32", "VOP2", "dpp8", 3),
            ("GLOBAL_LOAD_DWORD", "GLOBAL", "base", 20),
        ])

    def test_xml_without_instructions_is_an_error(self):
        with self.assertRaises(shader_scan.ScanError):
            shader_scan.parse_isa_xml("<Spec/>")

    def test_checked_in_table_has_every_form_once(self):
        forms = shader_scan.load_isa_forms(progress.ISA_FORMS)
        self.assertGreater(len(forms), 1000)
        self.assertEqual(len(forms), len(set(forms)))
        self.assertEqual({form for _, _, form, _ in forms}, set(shader_scan.FORMS))

    def test_regenerating_the_table_from_xml_round_trips(self):
        with tempfile.TemporaryDirectory() as directory:
            xml = Path(directory) / "isa.xml"
            xml.write_text(ISA_XML, encoding="utf-8")
            table = Path(directory) / "forms.txt"
            table.write_text("# header\n", encoding="utf-8")
            old = progress.ISA_FORMS
            progress.ISA_FORMS = table
            try:
                self.assertEqual(progress.regenerate_isa_forms(xml), 0)
            finally:
                progress.ISA_FORMS = old
            self.assertEqual(shader_scan.load_isa_forms(table), shader_scan.parse_isa_xml(ISA_XML))
            self.assertTrue(table.read_text(encoding="utf-8").startswith("# header\n"))


class ScanTests(unittest.TestCase):
    def test_known_answers_on_a_small_tree(self):
        with tempfile.TemporaryDirectory() as directory:
            write_tree(directory)
            result = shader_scan.measure(directory, ISA)
        self.assertEqual(result["instructions"], {"total": 14, "decoded": 10, "translated": 5})
        self.assertEqual(result["dpp"], {"total": 3, "decoded": 1})
        self.assertEqual(result["unsupported_throws"], 4)
        self.assertEqual(result["throw_areas"], [{"name": "Ir", "count": 3}, {"name": "Recompiler", "count": 1}])
        self.assertEqual(result["formats"], {"total": 5, "sampled": 2, "buffer": 2, "host": 4})
        self.assertEqual({f["encoding"] + " " + f["form"]: f["missing"] for f in result["forms"] if f["missing"]},
                         {"SOP1 base": ["S_NEW"], "VOP1 dpp16": ["V_CVT_F32_I32"], "VOP2 vop3": ["V_MAC_F32", "V_PAST_F32"], "VOP2 dpp8": ["V_ADD_F32"], "VOPC vop3": ["V_CMP_NEW"]})
        self.assertEqual({f["encoding"] + " " + f["form"]: f["untranslated"] for f in result["forms"] if f["untranslated"]},
                         {"VOP2 base": ["V_ADD_F32"], "VOP2 vop3": ["V_ADD_F32", "V_FIRST_F32", "V_LAST_F32"], "VOP1 vop3": ["V_MOV_B32"], "VOP2 dpp16": ["V_ADD_F32"]})
        self.assertEqual(result["untranslated"], ["V_ADD_F32", "V_FIRST_F32", "V_LAST_F32", "V_MOV_B32"])
        by_form = {(f["encoding"], f["form"]): (f["total"], f["decoded"], f["translated"]) for f in result["forms"]}
        self.assertEqual(by_form, {("SOP1", "base"): (2, 1, 1), ("VOP2", "base"): (1, 1, 0), ("VOP2", "vop3"): (5, 3, 0),
                                   ("VOP2", "dpp16"): (1, 1, 0), ("VOP2", "dpp8"): (1, 0, 0),
                                   ("VOP1", "base"): (1, 1, 1), ("VOP1", "vop3"): (2, 2, 1), ("VOP1", "dpp16"): (1, 0, 0),
                                   ("VOPC", "vop3"): (2, 1, 1), ("EXP", "base"): (1, 1, 1)})

    def test_fallthrough_case_labels_decode_to_the_opcode_their_group_returns(self):
        with tempfile.TemporaryDirectory() as directory:
            write_tree(directory)
            tables = Path(directory) / f"{DECODER}Tables.cpp"
            text = tables.read_text(encoding="utf-8")
            tables.write_text(text.replace("        case 0x03: return RdnaOpcode::SMovB32;\n",
                                           "        case 0x03:\n        case 0x09: return RdnaOpcode::SMovB32;\n"), encoding="utf-8")
            result = shader_scan.measure(directory, ISA)
        by_form = {(f["encoding"], f["form"]): (f["total"], f["decoded"], f["translated"]) for f in result["forms"]}
        self.assertEqual(by_form[("SOP1", "base")], (2, 2, 2))
        self.assertNotIn("SOP1 base", {f["encoding"] + " " + f["form"] for f in result["forms"] if f["missing"]})

    def test_unsupported_throws_skip_comments_rethrows_other_messages_and_tests(self):
        with tempfile.TemporaryDirectory() as directory:
            write_tree(directory)
            sites = shader_scan.unsupported_throws(shader_scan.read_tree(directory))
        self.assertEqual([(s["file"], s["line"]) for s in sites], [("Ir/Lower.cpp", 1), ("Ir/Lower.cpp", 6), ("Ir/Lower.cpp", 7), ("Top.cpp", 1)])
        self.assertEqual(sites[0]["message"], "Unsupported image type")

    def test_a_missing_opcode_table_stops_the_scan_naming_it(self):
        with tempfile.TemporaryDirectory() as directory:
            write_tree(directory, tables=[e for e in shader_scan.REQUIRED_TABLES if e != "SOP1"])
            with self.assertRaisesRegex(shader_scan.ScanError, "no SOP1 opcode table"):
                shader_scan.measure(directory, ISA)

    def test_a_missing_translator_stops_the_scan(self):
        with tempfile.TemporaryDirectory() as directory:
            write_tree(directory)
            (Path(directory) / "core/shader/recompiler/Translation/Translate.cpp").write_text("", encoding="utf-8")
            with self.assertRaisesRegex(shader_scan.ScanError, r"TranslationContext::emitScalar\(\)"):
                shader_scan.measure(directory, ISA)

    def test_comments_keep_offsets_and_literals(self):
        text = 'a // x\nb /* y\nz */ c "// not a comment"\n'
        stripped = shader_scan.strip_comments(text)
        self.assertEqual(len(stripped), len(text))
        self.assertEqual(stripped.count("\n"), text.count("\n"))
        self.assertIn('"// not a comment"', stripped)
        self.assertNotIn("x", stripped.replace("not a comment", ""))


class ReportTests(unittest.TestCase):
    SOURCE = {"isa": "AMD RDNA 2", "instructions": {"total": 5, "decoded": 3, "translated": 1},
              "dpp": {"total": 2, "decoded": 1}, "unsupported_throws": 3, "untranslated": ["V_ADD_F32"],
              "throw_areas": [{"name": "Translation", "count": 2}, {"name": "Recompiler", "count": 1}], "throw_sites": [],
              "formats": {"total": 10, "sampled": 6, "buffer": 5, "host": 4},
              "forms": [{"encoding": "SOP1", "form": "base", "total": 2, "decoded": 1, "translated": 1, "missing": ["S_NEW"], "untranslated": []},
                        {"encoding": "VOP2", "form": "vop3", "total": 3, "decoded": 2, "translated": 0, "missing": ["V_X"], "untranslated": ["V_A", "V_B"]},
                        {"encoding": "VOP2", "form": "dpp16", "total": 2, "decoded": 1, "translated": 0, "missing": ["V_Y"], "untranslated": ["V_Z"]}]}
    GROUP = {"percent": 100.0, "groups": [], "done": 0, "total": 0, "extra": []}

    def test_badge_shows_decoded_over_total_and_says_what_it_measures(self):
        svg = progress.shader_source_badge(self.SOURCE)
        self.assertIn("shader forms decoded", svg)
        self.assertIn("3/5", svg)
        self.assertNotIn("100", svg)

    def test_site_files_carry_the_source_numbers_and_the_wording(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "site"
            libraries = dict(self.GROUP)
            progress.write_site(output, libraries, dict(self.GROUP), self.SOURCE)
            data = json.loads((output / "progress.json").read_text(encoding="utf-8"))
            html = (output / "progress.html").read_text(encoding="utf-8")
            badge = (output / "badge-shaders.svg").read_text(encoding="utf-8")
        self.assertEqual(data["shader_source"]["instructions"], self.SOURCE["instructions"])
        self.assertEqual(data["shader_source"]["unsupported_throws"], 3)
        self.assertNotIn("untranslated", data["shader_source"])
        self.assertIn("counted from the recompiler's code, not from playing games", html)
        self.assertIn("3/5", badge)
        self.assertNotIn("1166", html)
        self.assertNotIn("GPU shader instructions", html)
        self.assertIn("<td>VOP2</td><td>dpp16</td><td>1</td><td>0</td><td>2</td></tr>", html)

    def test_progress_map_panel_shows_the_forms_not_the_old_instruction_names(self):
        svg = progress.render(dict(self.GROUP), dict(self.GROUP), self.SOURCE)
        self.assertIn("Shader forms decoded: 60.0% (3/5)", svg)
        self.assertIn("SOP1: 1/2", svg)
        self.assertIn("VOP2 in VOP3: 2/3", svg)
        self.assertNotIn("VOP2 dpp16: 1/2", svg)
        self.assertNotIn("GPU shader instructions", svg)
        self.assertIn("GPU shader instructions", progress.render(dict(self.GROUP), dict(self.GROUP)))

    def test_still_needed_groups_names_by_form_kind(self):
        data = progress.still_needed(self.SOURCE)
        self.assertEqual(data["lists"], [
            ("Instruction forms not decoded (2)", [("SOP1", ["S_NEW"]), ("VOP2 vop3", ["V_X"])]),
            ("DPP forms not decoded (1)", [("VOP2 dpp16", ["V_Y"])]),
            ("Decoded, no translator case (3)", [("VOP2 vop3", ["V_A", "V_B"]), ("VOP2 dpp16", ["V_Z"])]),
        ])
        self.assertEqual(data["throws"], (3, [("Translation", 2), ("Recompiler", 1)]))
        self.assertEqual(data["formats"], [("Sampled", 6, 10), ("Typed buffer", 5, 10), ("Vulkan host format", 4, 10)])

    def panel(self, svg):
        rows = re.findall(r'<rect x="([\d.]+)" y="([\d.]+)" width="([\d.]+)" height="14" fill="(#[0-9a-f]{6})"/>', svg)
        texts = re.findall(r'<text x="([\d.]+)" y="([\d.]+)" font-family="[\w-]+" font-size="\d+" fill="#ffffff" text-anchor="(\w+)">([^<]*)</text>', svg)
        return [(float(x), float(y), float(w), fill) for x, y, w, fill in rows], [(float(x), float(y), anchor, value) for x, y, anchor, value in texts]

    def test_still_needed_panel_lists_names_and_draws_the_bars(self):
        svg = progress.render(dict(self.GROUP), dict(self.GROUP), self.SOURCE)
        bars, texts = self.panel(svg)
        values = [t[3] for t in texts] + re.findall(r'paint-order="stroke">([^<]*)</text>', svg)
        for expected in ("Unsupported-throw sites by area (3)", "Instruction forms still needed", "Image formats (of 10)",
                         "Instruction forms not decoded (2)", "S_NEW", "V_X", "DPP forms not decoded (1)", "V_Y",
                         "Decoded, no translator case (3)", "V_A, V_B", "V_Z", "Translation", "Recompiler",
                         "Sampled", "6/10", "Typed buffer", "5/10", "Vulkan host format", "4/10",
                         "Still needed (counted from the code, not from playing games)"):
            self.assertIn(expected, values)
        self.assertRegex(svg, r'viewBox="0 0 1000 (\d+)" width="1000" height="\1"')
        track = 495 - 170 - 64 - 8
        fills = {color: [w for x, y, w, c in bars if c == color] for color in ("#d29922", "#2ea043")}
        self.assertEqual(fills["#d29922"], [round(track * 2 / 3, 2), round(track * 1 / 3, 2)])
        self.assertEqual(fills["#2ea043"], [round(track * 6 / 10, 2), round(track * 5 / 10, 2), round(track * 4 / 10, 2)])
        tracks = [(x, y) for x, y, w, c in bars if c == "#6e7681" and w == track]
        self.assertEqual(len(tracks), 5)
        left = [t for t in texts if t[0] < 500]
        for x, y, anchor, value in texts:
            for bx, by, bw, color in bars:
                if color == "#6e7681" and bw == track:
                    self.assertFalse(bx <= x <= bx + bw and by <= y - 3 <= by + 14, f"{value!r} drawn over a bar")
        counts = [t for t in left if t[2] == "end"]
        self.assertEqual([t[3] for t in counts], ["2", "1", "6/10", "5/10", "4/10"])
        self.assertTrue(all(t[0] == 491 for t in counts))

    def test_still_needed_panel_ignores_the_shared_palette(self):
        import shutil
        svg = progress.render(dict(self.GROUP), dict(self.GROUP), self.SOURCE)
        panel = svg[svg.index("Unsupported-throw sites by area"):]
        source = (TOOLS / "progress.py").read_text(encoding="utf-8")
        recolored_source = re.sub(r"^(DONE_COLOR, TODO_COLOR\b.*)$", lambda m: m.group(1) + '\nDONE_COLOR, TODO_COLOR = "#00ff00", "#da3633"', source, count=1, flags=re.M)
        self.assertNotEqual(recolored_source, source)
        with tempfile.TemporaryDirectory() as directory:
            for module in TOOLS.glob("*.py"):
                shutil.copy(module, Path(directory) / module.name)
            (Path(directory) / "progress.py").write_text(recolored_source, encoding="utf-8")
            spec = importlib.util.spec_from_file_location("anyps5_progress_palette", Path(directory) / "progress.py")
            recolored = importlib.util.module_from_spec(spec)
            sys.modules[spec.name] = recolored
            spec.loader.exec_module(recolored)
            other = recolored.render(dict(self.GROUP), dict(self.GROUP), self.SOURCE)
        self.assertEqual(other[other.index("Unsupported-throw sites by area"):], panel)
        self.assertNotIn("#da3633", panel)
        self.assertNotIn("#00ff00", panel)

    def test_still_needed_html_scales_to_the_totals_and_keeps_labels_beside_the_bars(self):
        html = progress.summary(dict(self.GROUP), dict(self.GROUP), self.SOURCE)
        for expected in ("<h2>Still needed</h2>", "<code>S_NEW</code>", "<code>V_A, V_B</code>", '<div class="cols">'):
            self.assertIn(expected, html)
        throws = re.findall(r'<td>(Translation|Recompiler)</td><td><div class="bar"><i style="width:([\d.]+)%;background:#d29922"></i></div></td><td class="n">(\d+)</td>', html)
        self.assertEqual(throws, [("Translation", "66.67", "2"), ("Recompiler", "33.33", "1")])
        formats = re.findall(r'<td>(Sampled|Typed buffer|Vulkan host format)</td><td><div class="bar"><i style="width:([\d.]+)%;background:#2ea043"></i></div></td><td class="n">(\d+/10)</td>', html)
        self.assertEqual(formats, [("Sampled", "60.00", "6/10"), ("Typed buffer", "50.00", "5/10"), ("Vulkan host format", "40.00", "4/10")])
        self.assertEqual(re.findall(r"\.bar\{background:(#[0-9a-f]+)", html), ["#6e7681"])
        section = re.search(r"<h2>Still needed</h2>.*?</div>\n</div>", html, re.S).group(0)
        self.assertNotIn("#da3633", section)

    def test_without_source_numbers_the_old_files_are_written(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "site"
            progress.write_site(output, dict(self.GROUP), dict(self.GROUP))
            data = json.loads((output / "progress.json").read_text(encoding="utf-8"))
        self.assertEqual(set(data), {"libraries", "shaders"})

    def fake_root(self, directory):
        root = Path(directory) / "source"
        (root / "core/libs/prx").mkdir(parents=True)
        opcodes = root / "core/shader/recompiler/RdnaDecoder/include/RdnaDecoder/RdnaOpcode.hpp"
        opcodes.parent.mkdir(parents=True)
        opcodes.write_text("enum class RdnaOpcode {\n    SEndpgm,\n};\n", encoding="utf-8")
        return root

    def run_main(self, *argv):
        import io
        from contextlib import redirect_stderr, redirect_stdout
        out, err = io.StringIO(), io.StringIO()
        with redirect_stdout(out), redirect_stderr(err):
            try:
                code = progress.main(list(argv))
            except SystemExit as raised:
                code = raised.code
        return code, out.getvalue(), err.getvalue()

    def test_unreadable_recompiler_leaves_the_numbers_out_unless_required(self):
        with tempfile.TemporaryDirectory() as directory:
            root = self.fake_root(directory)
            code, out, err = self.run_main("--root", str(root), "--json")
            self.assertEqual(code, 0)
            self.assertNotIn("shader_source", json.loads(out))
            self.assertIn("no SOP1 opcode table", err)
            code, out, err = self.run_main("--root", str(root), "--json", "--require-shader-source")
            self.assertEqual(code, 2)
            self.assertEqual(out, "")
            self.assertIn("no SOP1 opcode table", err)


if __name__ == "__main__":
    unittest.main()
