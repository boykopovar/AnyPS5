import argparse
import importlib.util
import hashlib
import json
import os
import re
import sys
import xml.etree.ElementTree as ET
import textwrap
from fractions import Fraction
from html import escape
from pathlib import Path

tokenizer_spec = importlib.util.spec_from_file_location("anyps5_cpp_tokenizer", Path(__file__).with_name("nid_names.py"))
tokenizer = importlib.util.module_from_spec(tokenizer_spec)
tokenizer_spec.loader.exec_module(tokenizer)
CPP_TOKEN = tokenizer.CPP_TOKEN
LITERALS, _, CODE_TOKEN = CPP_TOKEN.pattern.rpartition("|(?P<code>")
CODE_RUN = re.compile(LITERALS + "|(?P<code>(?:" + CODE_TOKEN[:-1] + r""")(?:(?![/"']|(?:u8|[uUL])?R?["'])(?:"""
                      + CODE_TOKEN[:-1] + r"|\s+))*)")
BRACE = re.compile(r"[{}]")
_NON_CODE_TRIGGER = re.compile(r'//|/\*|"|\'')
_RAW_BODY = re.compile(r'"(?P<delimiter>[^\s()\\]{0,16})\([\s\S]*?\)(?P=delimiter)"')
_STRING_BODY = re.compile(r'"(?:\\[\s\S]|[^"\\])*"')
_CHAR_BODY = re.compile(r"'(?:\\[\s\S]|[^'\\])*'")
_CODE_TOKEN = re.compile(r"(?:[0-9]|\.[0-9])[A-Za-z0-9_.']*|[A-Za-z_][A-Za-z0-9_]*|\S")
_PRE_TOKEN_CHARS = frozenset("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_.'")
_STRING_PREFIXES = frozenset(("u8", "u", "U", "L"))
_RAW_PREFIXES = frozenset(("R", "u8R", "uR", "UR", "LR"))
_BLANK_LINE_RUNS = re.compile(r"\n(?:[ \t]*\n)+")
_ANON_NAMESPACE = re.compile(r"\bnamespace\s*\{")
_CLASS_SCOPE = re.compile(r"\b(?:class|struct|union)\b[^;{}()]*\{")
_STATIC_KW = re.compile(r"\bstatic\b")
_BRACKET_DELIM = re.compile(r"[()\[\]{}]")
_STMT_DELIM = re.compile(r"[{};]")
_CV_QUAL = re.compile(r"\b(?:const|volatile)\b")
_TYPE_KW = re.compile(r"\b(?:struct|class|union|enum|typename|register)\b")
_SCOPE_SEP = re.compile(r"\s*::\s*")
_OP_PAIR = re.compile(r"->|<<|>>|<=|>=")
_IDENT = re.compile(r"[A-Za-z_]\w*")
_RETURN_IDENT = re.compile(r"return\s+(\w+)\s*;")
_WORD = re.compile(r"\w+")
_ASM_MACROS = tuple(sorted(tokenizer.ASM_FUNCTION_MACROS))

scan_spec = importlib.util.spec_from_file_location("anyps5_shader_scan", Path(__file__).with_name("shader_scan.py"))
shader_scan = importlib.util.module_from_spec(scan_spec)
sys.modules[scan_spec.name] = shader_scan
scan_spec.loader.exec_module(shader_scan)

isa_spec = importlib.util.spec_from_file_location("anyps5_isa_profiles", Path(__file__).with_name("isa_profiles.py"))
isa_profiles = importlib.util.module_from_spec(isa_spec)
isa_spec.loader.exec_module(isa_profiles)

ROOT = Path(__file__).resolve().parent.parent
PRX = ROOT / "core" / "libs" / "prx"
OPCODES = ROOT / "core" / "shader" / "recompiler" / "RdnaDecoder" / "include" / "RdnaDecoder" / "RdnaOpcode.hpp"
ISA = Path(__file__).resolve().parent / "rdna_isa.txt"
ISA_FORMS = Path(__file__).resolve().parent / "rdna2_isa_forms.txt"
ISA_GFX1013 = Path(__file__).resolve().parent / "gfx1013_isa_forms.txt"
SOURCE = f'https://github.com/{os.environ.get("GITHUB_REPOSITORY", "boykopovar/AnyPS5")}/blob/main'
DEFINITION = re.compile(r"\bAPS5_VABI\s+(\w+)\s*\([^;{]*\)\s*(?:noexcept\s*)?(?:try\s*)?\{")
STATIC_DECLARATION = re.compile(r"\bstatic\b[^;{}]*\bAPS5_VABI\s+(\w+)\s*\([^;{]*\)\s*(?:noexcept\s*)?;")
_VABI_DECLARATION = re.compile(r"\bAPS5_VABI\s+(\w+)\s*\([^;{]*\)\s*(?:noexcept\s*)?;")
EXPORT = re.compile(r"\bAPS5_EXPORT\s*\(\s*LITERAL\s*,\s*(\w+)\s*\)")
ASM_DEFINITION = re.compile(rf'\b(?:{"|".join(sorted(tokenizer.ASM_FUNCTION_MACROS))})\s*\(\s*"(\w+)"\s*\)')
STUB = "NotImplemented_nid_no_patch"
CALL = re.compile(r"\b(\w+)\s*\(")
KEYWORDS = r"return|else|do|throw|co_return|co_await|co_yield|and|or|not|xor|bitand|bitor|compl"
_KEYWORDS_SET = frozenset(KEYWORDS.split("|"))
QUALIFIED = re.compile(rf"(?:\.|->|(?:\b(?!(?:{KEYWORDS})\b)\w+|>)\s*::)\s*$")
CMAKE_SHARED = re.compile(r'^\s*include\(\s*"?\$\{CMAKE_CURRENT_SOURCE_DIR\}/\.\./([\w.-]+)/[\w.-]+\.cmake"?\s*\)', re.M)
SOURCE_SHARED = re.compile(r'^\s*#\s*include\s*"prx/([\w.-]+)/[\w./-]+\.cpp"', re.M)
STUB_WRAPPER = re.compile(
    r"^(?=[\w:<>,\s*&\[\]]*\()[ \t]*(?:static\s+)?(?:\[\[noreturn\]\]\s+)?(?:(?!APS5_VABI)[\w:<>,\s*&])+?[\s*&]"
    r"(?!(?:if|while|for|switch|catch|return|else)\b)(\w+)\s*\([^;{]*\)\s*"
    r"(?:const\s*)?(?:noexcept\s*)?(?:->\s*[\w:<>*&\s]+?)?\{", re.M)
_WRAPPER_CANDIDATE = re.compile(r"\)\s*(?:const\s*)?(?:noexcept\s*)?(?:->\s*[\w:<>*&\s]+?)?\{")
DISCARD = re.compile(r"\(void\)\s*\w+\s*;")
EMPTY_RETURN = re.compile(r"return\s*;")
CONST_RETURN = re.compile(r"return\s+(?:[+-]?\s*(?:0[xX][0-9a-fA-F]+|\d+)[uUlL]*|\w+|\"(?:\\.|[^\"\\])*\")\s*;")
WRAPPER_RETURN = re.compile(r"return\s+(\w+)\s*\(([^()]*)\)\s*;")
BARE_THROW = re.compile(r"throw\b[^;]*;")
FLAT_SEGMENTS = ("GLOBAL_", "SCRATCH_")
OPCODE_SENTINELS = {"Invalid", "Count", "Unknown", "Unsupported"}
OPCODE_ALIASES = {
    "TBufferLoadFormatX": "TBUFFER_LOAD_FORMAT_X",
    "TBufferLoadFormatXyzw": "TBUFFER_LOAD_FORMAT_XYZW",
    "ExpMrt": "EXP",
    "ExpPos": "EXP",
    "ExpParam": "EXP",
    "VAddcU32": "V_ADD_CO_CI_U32",
    "VMadMixloF16": "V_FMA_MIXLO_F16",
    "VMadMixhiF16": "V_FMA_MIXHI_F16",
}
OPCODE_VARIANTS = {
    "SEndpgm": ("S_ENDPGM_SAVED", "S_ENDPGM_ORDERED_PS_DONE", "S_SETKILL"),
    "VFmaF32": ("V_FMA_MIX_F32",),
    "SAddI32": ("S_ADDK_I32",),
    "SCmpEqI32": ("S_CMPK_EQ_I32",),
    "SCmpLgI32": ("S_CMPK_LG_I32",),
    "SCmpGtI32": ("S_CMPK_GT_I32",),
    "SCmpGeI32": ("S_CMPK_GE_I32",),
    "SCmpLtI32": ("S_CMPK_LT_I32",),
    "SCmpLeI32": ("S_CMPK_LE_I32",),
    "SCmpEqU32": ("S_CMPK_EQ_U32",),
    "SCmpLgU32": ("S_CMPK_LG_U32",),
    "SCmpGtU32": ("S_CMPK_GT_U32",),
    "SCmpGeU32": ("S_CMPK_GE_U32",),
    "SCmpLtU32": ("S_CMPK_LT_U32",),
    "SCmpLeU32": ("S_CMPK_LE_U32",),
    "SWaitcnt": ("S_WAITCNT_VSCNT", "S_WAITCNT_VMCNT", "S_WAITCNT_EXPCNT", "S_WAITCNT_LGKMCNT"),
    "STtracedata": ("S_TTRACEDATA_IMM",),
    "SCbranchCdbg": ("S_CBRANCH_CDBGSYS", "S_CBRANCH_CDBGUSER", "S_CBRANCH_CDBGSYS_OR_USER", "S_CBRANCH_CDBGSYS_AND_USER"),
    "VAddI32": ("V_ADD_CO_U32",),
    "VSubI32": ("V_SUB_CO_U32",),
    "VSubrevI32": ("V_SUBREV_CO_U32",),
    "VMacF32": ("V_FMAC_F32",),
    "VMadmkF32": ("V_FMAMK_F32",),
    "VMadakF32": ("V_FMAAK_F32",),
    "VMacLegacyF32": ("V_FMAC_LEGACY_F32",),
    "VMadLegacyF32": ("V_FMA_LEGACY_F32",),
    "ImageSample": ("IMAGE_SAMPLE_L", "IMAGE_SAMPLE_B", "IMAGE_SAMPLE_C_LZ", "IMAGE_SAMPLE_L_O", "IMAGE_SAMPLE_D_CL_O"),
}
REPORT_ROWS = 100
PANEL_WIDTH, GAP, MAP_HEIGHT, HEADER = 495, 10, 280, 30
LIBRARY_TIERS = ("Large system libraries", "Medium system libraries", "Small system libraries")
GRID_COLUMNS, GRID_ROW = 12, 28
DONE_COLOR, TODO_COLOR, BORDER, TEXT = "#2ea043", "#6e7681", "#0d1117", "#ffffff"


def unique_json_keys(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError(f"duplicate JSON key {key!r}")
        result[key] = value
    return result


def _blank_span(span):
    if "\n" not in span:
        return " " * len(span)
    return "\n".join(" " * len(line) for line in span.split("\n"))


def _token_before(text, index, floor):
    scan_start = index
    while scan_start > floor and text[scan_start - 1] in _PRE_TOKEN_CHARS:
        scan_start -= 1
    if scan_start == index:
        return None, None, index
    last_start = index
    last_token = None
    for token in _CODE_TOKEN.finditer(text, scan_start):
        if token.start() >= index:
            break
        if token.end() > index:
            return token.start(), token.group(), token.end()
        last_start = token.start()
        last_token = token.group()
        if token.end() == index:
            return last_start, last_token, index
    return None, None, index


def blank(match):
    if match.group("code") is not None:
        return match.group()
    if match.group("comment") is None:
        return "LITERAL"
    return _blank_span(match.group())


def code(text):
    if "\\\n" in text:
        text = text.replace("\\\n", "")
    if "\\\r\n" in text:
        text = text.replace("\\\r\n", "")
    out = []
    pos = 0
    cursor = 0
    floor = 0
    no_block_close = False
    length = len(text)
    while cursor < length:
        trigger = _NON_CODE_TRIGGER.search(text, cursor)
        if trigger is None:
            break
        index = trigger.start()
        token = trigger.group()
        if token == "//":
            end = text.find("\n", index + 2)
            if end == -1:
                end = length
            out.append(text[pos:index])
            out.append(" " * (end - index))
            pos = cursor = floor = end
        elif token == "/*":
            if no_block_close:
                cursor = floor = index + 2
                continue
            close = text.find("*/", index + 2)
            if close == -1:
                no_block_close = True
                cursor = floor = index + 2
                continue
            end = close + 2
            out.append(text[pos:index])
            out.append(_blank_span(text[index:end]))
            pos = cursor = floor = end
        elif token == '"':
            tok_start, tok_text, tok_end = _token_before(text, index, floor)
            if tok_end > index:
                cursor = floor = tok_end
                continue
            if tok_text in _RAW_PREFIXES:
                raw_match = _RAW_BODY.match(text, index)
                if raw_match is not None:
                    end = raw_match.end()
                    out.append(text[pos:tok_start])
                    out.append("LITERAL")
                    pos = cursor = floor = end
                    continue
            str_match = _STRING_BODY.match(text, index)
            if str_match is not None:
                start = tok_start if tok_text in _STRING_PREFIXES else index
                end = str_match.end()
                out.append(text[pos:start])
                out.append("LITERAL")
                pos = cursor = floor = end
            else:
                cursor = floor = index + 1
        else:
            tok_start, tok_text, tok_end = _token_before(text, index, floor)
            if tok_end > index:
                cursor = floor = tok_end
                continue
            char_match = _CHAR_BODY.match(text, index)
            if char_match is not None:
                start = tok_start if tok_text in _STRING_PREFIXES else index
                end = char_match.end()
                out.append(text[pos:start])
                out.append("LITERAL")
                pos = cursor = floor = end
            else:
                cursor = floor = index + 1
    if out:
        out.append(text[pos:])
        text = "".join(out)
    return _BLANK_LINE_RUNS.sub("\n", text) if "\n" in text else text


def body_end(text, start):
    if start < len(text) and text[start] == "{":
        close = text.find("}", start + 1)
        if close < 0:
            return len(text)
        nxt = text.find("{", start + 1, close)
        if nxt < 0:
            return close
        depth = 2
        pos = nxt + 1
        while True:
            nxt = text.find("{", pos, close)
            if nxt >= 0:
                depth += 1
                pos = nxt + 1
                continue
            depth -= 1
            if depth == 0:
                return close
            pos = close + 1
            close = text.find("}", pos)
            if close < 0:
                return len(text)
    depth = 0
    for brace in BRACE.finditer(text, start):
        if brace.group() == "{":
            depth += 1
        else:
            depth -= 1
            if depth == 0:
                return brace.start()
    return len(text)


def brace_spans(text, pattern):
    finder = pattern.finditer(text) if isinstance(pattern, re.Pattern) else re.finditer(pattern, text)
    return [(match.end() - 1, body_end(text, match.end() - 1)) for match in finder]


def _anon_namespace_spans(text):
    spans = []
    pos = 0
    length = len(text)
    while True:
        idx = text.find("namespace", pos)
        if idx < 0:
            return spans
        end = idx + 9
        if idx == 0 or not (text[idx - 1].isalnum() or text[idx - 1] == "_"):
            cur = end
            while cur < length and text[cur].isspace():
                cur += 1
            if cur < length and text[cur] == "{":
                spans.append((cur, body_end(text, cur)))
                pos = cur + 1
                continue
        pos = end


class Linkage:
    def __init__(self, text):
        self.text = text
        self.anonymous = _anon_namespace_spans(text) if "namespace" in text else []
        self._classes = None
        self.declared = set()
        if "static" in text and "APS5_VABI" in text:
            for match in _VABI_DECLARATION.finditer(text):
                start = match.start()
                boundary = max(text.rfind(";", 0, start), text.rfind("{", 0, start), text.rfind("}", 0, start)) + 1
                static_match = _STATIC_KW.search(text, boundary, start)
                if static_match is not None and self.namespace_scope(static_match.start()):
                    self.declared.add(match.group(1))

    @property
    def classes(self):
        if self._classes is None:
            self._classes = (brace_spans(self.text, _CLASS_SCOPE)
                             if ("class" in self.text or "struct" in self.text or "union" in self.text) else [])
        return self._classes

    def namespace_scope(self, start):
        return not any(open_brace < start < close for open_brace, close in self.classes)

    def internal(self, name, start):
        if any(open_brace < start < close for open_brace, close in self.anonymous):
            return True
        if name not in self.declared:
            boundary = max(self.text.rfind(character, 0, start) for character in ";{}")
            if not _STATIC_KW.search(self.text, boundary + 1, start):
                return False
        return self.namespace_scope(start)


def _iter_stub_wrappers(text):
    if "(" not in text or "{" not in text:
        return
    position = 0
    for candidate in _WRAPPER_CANDIDATE.finditer(text):
        end = candidate.end()
        if end <= position:
            continue
        param_end = candidate.start()
        bound = max(position, text.rfind(";", position, param_end) + 1, text.rfind("{", position, param_end) + 1)
        if bound == 0 or text[bound - 1] == "\n":
            search_start = bound
        else:
            newline = text.find("\n", bound, param_end)
            if newline < 0:
                continue
            search_start = newline + 1
        if "(" not in text[search_start:param_end]:
            continue
        match = STUB_WRAPPER.search(text, search_start, end)
        if match is not None:
            yield match
            position = match.end()


def stub_calls(text):
    if STUB not in text:
        return {STUB}
    helpers = {}
    for match in _iter_stub_wrappers(text):
        body = text[match.end() - 1:body_end(text, match.end() - 1)]
        helpers.setdefault(match.group(1), set()).update(call.group(1) for call in CALL.finditer(body))
    calls = {STUB}
    while True:
        found = {name for name, targets in helpers.items() if name not in calls and targets & calls}
        if not found:
            return calls
        calls |= found


TYPE_DECLARATION = re.compile(r"\b(?:struct|class|union|enum(?:\s+class|\s+struct)?)\s+(?:\w+\s*::\s*)*(\w+)\s*"
                              r"(?:final\s*)?(?::(?!:)([^{;]*))?[{;]")
TYPE_ALIAS = re.compile(r"\busing\s+(\w+)\s*=\s*([^;]*);")
TYPEDEF = re.compile(r"\btypedef\b([^;{}]*?)\b(\w+)\s*;")
BRACED_NAME = re.compile(r"\}\s*(\w+)\s*;")
CONVERSION = re.compile(r"\boperator\s+(?:(?:const|volatile)\s+)*(?:\w+\s*::\s*)*(\w+)\s*(?:<[^;{}]*>)?\s*[*&]")
SIMPLE_TYPE = re.compile(r"\s*(?:typename\s+)?(?:\w+\s*::\s*)*(\w+)\s*")
TEMPLATE_CLAUSE = re.compile(r"\btemplate\s*<([^{};]*)>")
TEMPLATE_PARAMETER = re.compile(r"\b(?:typename|class)\b\s*(?:\.\.\.)?\s*(\w+)")
DECLARATION = re.compile(r"(?<![\w.:>])((?:\w+\s*::\s*)*\w+)(\s*<[^<>;(){}]*>)?((?:\s*(?:\bconst\b|\bvolatile\b|\*|&))*)"
                         r"\s*\b(\w+)(?=\s*(?:[=;,)({\[]|:(?!:)))")
ARGUMENT = re.compile(r"([*&]?)\s*(\w+)")
PARAMETER = re.compile(r"\s*((?:\w+\s*::\s*)*\w+)\s*([*&\s]*)(\w+)?\s*")
STATEMENT_WORDS = frozenset(("return", "else", "do", "throw", "new", "delete", "case", "goto", "co_return", "co_await",
                             "co_yield", "not", "and", "or", "xor", "bitand", "bitor", "compl", "sizeof", "alignof",
                             "using", "namespace", "public", "private", "protected", "friend", "operator", "template",
                             "typename", "struct", "class", "union", "enum", "if", "while", "for", "switch", "catch",
                             "static_assert", "default"))
INDEFINITE_TYPES = frozenset(("auto", "const", "volatile", "static", "inline", "constexpr", "constinit", "register",
                              "extern", "mutable", "thread_local", "explicit", "virtual", "unsigned", "signed"))


def closing(text, start):
    depth = 0
    for match in _BRACKET_DELIM.finditer(text, start):
        if match.group() in "([{":
            depth += 1
        else:
            depth -= 1
            if depth == 0:
                return match.start()
    return len(text)


class TypeIndex:
    def __init__(self, texts):
        self.parent, self.known, self.opaque = {}, set(), set()
        for text in texts:
            if "struct" in text or "class" in text or "union" in text or "enum" in text:
                for match in TYPE_DECLARATION.finditer(text):
                    self.known.add(match.group(1))
                    if match.group(2):
                        for base in _WORD.findall(match.group(2)):
                            if base not in ("public", "protected", "private", "virtual"):
                                self.join(match.group(1), base)
            if "using" in text:
                for match in TYPE_ALIAS.finditer(text):
                    self.alias(match.group(1), match.group(2))
            if "typedef" in text:
                for match in TYPEDEF.finditer(text):
                    self.alias(match.group(2), match.group(1))
            if "}" in text:
                self.opaque.update(match.group(1) for match in BRACED_NAME.finditer(text))
            if "operator" in text:
                self.opaque.update(match.group(1) for match in CONVERSION.finditer(text))
        roots = {self.find(name) for name in self.known}
        self.concrete = {name for name in self.parent if self.find(name) in roots} | self.known
        opaque_roots = {self.find(o) for o in self.opaque}
        self.concrete -= {name for name in self.concrete if self.find(name) in opaque_roots}

    def find(self, name):
        self.parent.setdefault(name, name)
        while self.parent[name] != name:
            self.parent[name] = self.parent[self.parent[name]]
            name = self.parent[name]
        return name

    def join(self, first, second):
        self.parent[self.find(first)] = self.find(second)

    def alias(self, name, target):
        simple = SIMPLE_TYPE.fullmatch(target)
        if simple:
            self.join(name, simple.group(1))
        else:
            self.opaque.add(name)

    def related(self, first, second):
        return self.find(first) == self.find(second)


def split_arguments(text):
    parts, depth, start = [], 0, 0
    for i, character in enumerate(text):
        if character in "([{":
            depth += 1
        elif character in ")]}":
            depth -= 1
        elif character == "," and not depth:
            parts.append(text[start:i])
            start = i + 1
    return parts + [text[start:]]


def split_parameters(text):
    parts, depth, angle, start, default, reliable = [], 0, 0, 0, None, True
    for i, character in enumerate(text):
        if character in "([{":
            depth += 1
        elif character in ")]}":
            depth -= 1
        elif depth:
            continue
        elif default is not None:
            reliable = reliable and character not in "<>"
            if character == ",":
                parts.append((default, True))
                start, default = i + 1, None
        elif character == "<":
            angle += 1
        elif character == ">" and angle:
            angle -= 1
        elif character == "=" and not angle:
            default = text[start:i]
        elif character == "," and not angle:
            parts.append((text[start:i], False))
            start = i + 1
    parts.append((default, True) if default is not None else (text[start:], False))
    return parts, reliable and not angle


def parameter_type(declaration):
    if "(" in declaration or "[" in declaration or "<" in declaration or "..." in declaration:
        return None
    plain = _CV_QUAL.sub(" ", declaration)
    qualified = plain != declaration
    plain = _TYPE_KW.sub(" ", plain)
    match = PARAMETER.fullmatch(plain)
    if not match:
        return None
    base = _SCOPE_SEP.split(match.group(1))[-1]
    marks = match.group(2).replace(" ", "").replace("\t", "").replace("\n", "")
    if "&&" in marks or (marks and set(marks) - {"*", "&"}):
        return None
    if "&" in marks:
        return None if qualified or marks.count("&") > 1 else ("reference", base, marks.count("*"))
    if "*" in marks:
        return ("pointer", base, marks.count("*"))
    return None


class Definition:
    def __init__(self, name, text, match, follow_qualified):
        self.name, self.follow_qualified, self.start = name, follow_qualified, match.start()
        self._text = text
        self._match = match
        self._parsed_signature = False
        self._parameters = None
        self._required = 0
        self._maximum = 0
        self._templates = None
        self.signature = match.group()
        self.body = text[match.end() - 1:body_end(text, match.end() - 1)]
        self.calls, self.reaches, self.variables_cache = [], False, None

    def _ensure_signature(self):
        text = self._text
        match = self._match
        start = match.end(1)
        opening = text.index("(", start)
        parameters = text[opening + 1:closing(text, opening)]
        self._parameters, self._required, self._maximum = [], 0, 0
        if parameters.strip() not in ("", "void"):
            declarations, reliable = split_parameters(parameters)
            for declaration, has_default in declarations:
                pack = "..." in declaration
                self._parameters.append(None if pack else parameter_type(declaration))
                self._required += not (pack or has_default)
                self._maximum = float("inf") if pack or self._maximum == float("inf") else self._maximum + 1
            if not reliable:
                self._parameters, self._required, self._maximum = [], 0, float("inf")
        boundary = max(text.rfind(character, 0, match.start(1)) for character in ";{}") + 1
        self._templates = {parameter for clause in TEMPLATE_CLAUSE.findall(text, boundary, match.start(1))
                           for parameter in TEMPLATE_PARAMETER.findall(clause)}
        self._parsed_signature = True
        self._text = None
        self._match = None

    @property
    def parameters(self):
        if not self._parsed_signature:
            self._ensure_signature()
        return self._parameters

    @property
    def required(self):
        if not self._parsed_signature:
            self._ensure_signature()
        return self._required

    @property
    def maximum(self):
        if not self._parsed_signature:
            self._ensure_signature()
        return self._maximum

    @property
    def templates(self):
        if not self._parsed_signature:
            self._ensure_signature()
        return self._templates

    def variables(self):
        if self.variables_cache is None:
            found = {}
            for text in (self.signature, self.body):
                for match in DECLARATION.finditer(text):
                    base = _SCOPE_SEP.split(match.group(1))[-1]
                    if base in STATEMENT_WORDS or base[0].isdigit():
                        continue
                    following = text[match.end():match.end() + 64].lstrip()
                    kind = None
                    if base not in INDEFINITE_TYPES and not match.group(2) and not following.startswith("["):
                        kind = (base, match.group(3).count("*"))
                    found.setdefault(match.group(4), set()).add(kind)
            self.variables_cache = {name: next(iter(kinds)) if len(kinds) == 1 else None for name, kinds in found.items()}
        return self.variables_cache

    def argument(self, text):
        match = ARGUMENT.fullmatch(text.strip())
        if not match:
            return None
        kind = self.variables().get(match.group(2))
        if kind is None:
            return None
        depth = kind[1] + {"*": -1, "&": 1, "": 0}[match.group(1)]
        return (kind[0], depth) if depth >= 0 else None


def accepts(definition, argument, parameter, types):
    if parameter is None or argument is None:
        return True
    kind, base, depth = parameter
    argument_base, argument_depth = argument
    if argument_base not in types.concrete or (kind == "pointer" and not argument_depth):
        return True
    generic = base in definition.templates or base not in types.concrete
    if generic:
        return depth <= argument_depth
    return depth == argument_depth and types.related(base, argument_base)


def call_arguments(body, opening):
    end = closing(body, opening)
    inner = body[opening + 1:end]
    top, depth = [], 0
    for character in inner:
        if character in "([{":
            depth += 1
        elif character in ")]}":
            depth -= 1
        elif not depth:
            top.append(character)
    top = _OP_PAIR.sub(" ", "".join(top))
    if "..." in top or ("<" in top and ">" in top):
        return None
    return [] if not inner.strip() else split_arguments(inner)


def resolve(caller, arguments, candidates, types):
    if arguments is None:
        return candidates
    count = len(arguments)
    fitting = [c for c in candidates if c.required <= count <= c.maximum] or candidates
    if len(fitting) == 1 or not any(c.parameters for c in fitting):
        return fitting
    kinds = [caller.argument(argument) for argument in arguments]
    if not any(kinds):
        return fitting
    if callable(types):
        types = types()
    typed = [c for c in fitting if all(accepts(c, kind, parameter, types)
                                       for kind, parameter in zip(kinds, c.parameters))]
    return typed or fitting


def _is_qualified(body, pos):
    end = pos
    while end > 0 and body[end - 1].isspace():
        end -= 1
    if end == 0:
        return False
    ch = body[end - 1]
    if ch == ".":
        return True
    if ch == ">":
        return end >= 2 and body[end - 2] == "-"
    if ch == ":" and end >= 2 and body[end - 2] == ":":
        idx = end - 2
        while idx > 0 and body[idx - 1].isspace():
            idx -= 1
        if idx == 0:
            return False
        if body[idx - 1] == ">":
            return True
        word_end = idx
        while idx > 0 and (body[idx - 1].isalnum() or body[idx - 1] == "_"):
            idx -= 1
        return idx < word_end and body[idx:word_end] not in _KEYWORDS_SET
    return False


def reaching_definitions(texts, headers, asm_names):
    exports, files = {}, []
    has_stub = False
    for text in texts:
        if not has_stub and STUB in text:
            has_stub = True
        helpers = {}
        for match in _iter_stub_wrappers(text):
            helpers.setdefault(match.group(1), []).append(Definition(match.group(1), text, match, True))
        definitions = ([Definition(match.group(1), text, match, False) for match in DEFINITION.finditer(text)]
                       if "APS5_VABI" in text else [])
        for definition in definitions:
            exports.setdefault(definition.name, []).append(definition)
        files.append((helpers, definitions))
    if not has_stub:
        return files
    export_names = set(exports)
    export_names.add(STUB)
    nodes = []
    raw_calls = {}
    approx_callers = {}
    direct_stub = set()
    for helpers, definitions in files:
        local = [d for group in helpers.values() for d in group] + definitions
        known = export_names | helpers.keys() if helpers else export_names
        for definition in local:
            body = definition.body
            if "(" in body:
                entries = []
                for call in CALL.finditer(body):
                    name = call.group(1)
                    if name not in known:
                        continue
                    if not definition.follow_qualified and _is_qualified(body, call.start()):
                        continue
                    if name == STUB:
                        entries = None
                        direct_stub.add(id(definition))
                        break
                    targets = helpers.get(name, [])
                    export_targets = exports.get(name, []) if name not in asm_names else []
                    entries.append((call.end() - 1, targets, export_targets))
                if entries is None:
                    definition.calls.append(None)
                elif entries:
                    raw_calls[id(definition)] = entries
                    for _, targets, export_targets in entries:
                        for target in targets:
                            approx_callers.setdefault(id(target), []).append(definition)
                        for target in export_targets:
                            approx_callers.setdefault(id(target), []).append(definition)
            nodes.append(definition)
    can_reach = set(direct_stub)
    approx_pending = [node for node in nodes if id(node) in direct_stub]
    while approx_pending:
        node = approx_pending.pop()
        for caller in approx_callers.get(id(node), ()):
            caller_id = id(caller)
            if caller_id not in can_reach:
                can_reach.add(caller_id)
                approx_pending.append(caller)
    types = None

    def get_types():
        nonlocal types
        if types is None:
            loaded_headers = headers() if callable(headers) else headers
            types = TypeIndex(texts + loaded_headers)
        return types

    callers = {}
    pending = []
    for node in nodes:
        node_id = id(node)
        if node_id not in can_reach:
            continue
        if node_id in direct_stub:
            pending.append(node)
            continue
        body = node.body
        for paren_pos, targets, export_targets in raw_calls.get(node_id, ()):
            if len(targets) > 1:
                if not any(id(target) in can_reach for target in targets):
                    targets = []
                elif not all(id(target) in direct_stub for target in targets):
                    targets = resolve(node, call_arguments(body, paren_pos), targets, get_types)
            node.calls.append((targets, export_targets))
            for target in targets:
                callers.setdefault(id(target), []).append(node)
            for target in export_targets:
                callers.setdefault(id(target), []).append(node)
        pending.append(node)
    while pending:
        node = pending.pop()
        if node.reaches or not any(call is None or any(t.reaches for t in call[0]) or
                                   (call[1] and all(t.reaches for t in call[1])) for call in node.calls):
            continue
        node.reaches = True
        pending.extend(callers.get(id(node), ()))
    return files


def _iter_statements(body):
    depth, start = 0, 1
    for match in _STMT_DELIM.finditer(body, 1):
        character = match.group()
        if character == "{":
            depth += 1
        elif character == "}":
            depth -= 1
        elif not depth:
            end = match.end()
            yield body[start:end]
            start = end
    yield body[start:]


def statements(body):
    return list(_iter_statements(body))


def template_open(tokens, index):
    before = tokens[index - 1] if index else ""
    after = tokens[index + 1] if index + 1 < len(tokens) else ""
    return bool(_IDENT.fullmatch(before)) and after not in ("<", "=")


def inert_parameters(signature):
    parameters, nesting, default = set(), [], False
    text = signature.partition("(")[2].rpartition(")")[0]
    tokens = [match.group() for match in CPP_TOKEN.finditer(text)]
    for index, token in enumerate(tokens):
        if token == "," and not nesting:
            default = False
        elif token == "=" and not nesting:
            default = True
        elif token == "<" and default and not template_open(tokens, index):
            continue
        elif token in ("(", "[", "{", "<"):
            nesting.append(token)
        elif token in (")", "]", "}", ">") and nesting:
            if nesting[-1] == {")": "(", "]": "[", "}": "{", ">": "<"}[token]:
                nesting.pop()
        elif not default and not any(part in ("[", "{", "<") for part in nesting):
            if _IDENT.fullmatch(token):
                parameters.add(token)
    return parameters


def inert_statement(statement, wrappers, parameters):
    if EMPTY_RETURN.fullmatch(statement) or CONST_RETURN.fullmatch(statement) or BARE_THROW.fullmatch(statement):
        returned = _RETURN_IDENT.fullmatch(statement)
        return returned is None or returned.group(1) not in parameters
    call = WRAPPER_RETURN.fullmatch(statement)
    return bool(call) and call.group(1) in wrappers


def _classify_inert(body, wrappers, signature):
    parameters = None
    terminals = 0
    wrapper_target = None
    for statement in _iter_statements(body):
        statement = statement.strip()
        if not statement:
            continue
        if DISCARD.fullmatch(statement):
            continue
        if terminals:
            return False, None
        if EMPTY_RETURN.fullmatch(statement) or CONST_RETURN.fullmatch(statement) or BARE_THROW.fullmatch(statement):
            returned = _RETURN_IDENT.fullmatch(statement)
            if returned is not None:
                if parameters is None:
                    parameters = inert_parameters(signature)
                if returned.group(1) in parameters:
                    return False, None
            terminals = 1
            continue
        call = WRAPPER_RETURN.fullmatch(statement)
        if call:
            target = call.group(1)
            if target in wrappers:
                terminals = 1
                continue
            wrapper_target = target
            terminals = 1
            continue
        return False, None
    return wrapper_target is None, wrapper_target


def inert_body(body, wrappers, signature):
    is_inert, _ = _classify_inert(body, wrappers, signature)
    return is_inert


def inert_wrappers(helpers):
    names = set()
    pending = {}
    for name, group in helpers.items():
        latest = group[-1]
        is_inert, target = _classify_inert(latest.body, names, latest.signature)
        if is_inert:
            names.add(name)
        elif target is not None and target in helpers:
            pending[name] = target
    while pending:
        found = [name for name, target in pending.items() if target in names]
        if not found:
            break
        for name in found:
            names.add(name)
            del pending[name]
    return names


def shared_source_owners(path):
    cmake = path / "CMakeLists.txt"
    texts = [(CMAKE_SHARED, cmake.read_text(encoding="utf-8", errors="ignore"))] if cmake.is_file() else []
    texts += [(SOURCE_SHARED, source.read_text(encoding="utf-8", errors="ignore")) for source in sorted(path.glob("*.cpp"))]
    owners = [m[1] for pattern, content in texts for m in pattern.finditer(content)]
    return list(dict.fromkeys(owner for owner in owners if owner != path.name and (path.parent / owner).is_dir()))


def _scan_library_files(path, extensions):
    found = []
    stack = [os.fspath(path)]
    while stack:
        current = stack.pop()
        with os.scandir(current) as entries:
            for entry in entries:
                if entry.is_dir(follow_symlinks=False):
                    if entry.name != "tests":
                        stack.append(entry.path)
                elif entry.name.endswith(extensions) and entry.is_file():
                    found.append(entry.path)
    return found


def scan_library(path):
    done, todo, inert, live, assembled = set(), set(), set(), set(), set()
    sources = []
    for source_path in _scan_library_files(path, (".cpp",)):
        with open(source_path, encoding="utf-8", errors="ignore") as handle:
            sources.append(handle.read())
    texts = [code(source) for source in sources]
    exported = {match.group(1) for text in texts if "APS5_EXPORT" in text for match in EXPORT.finditer(text)}
    for source in sources:
        if not any(macro in source for macro in _ASM_MACROS):
            continue
        definitions = {match.start(): match.group(1) for match in ASM_DEFINITION.finditer(source)}
        if not definitions:
            continue
        for token in CPP_TOKEN.finditer(source):
            if token.group("code") is None or token.start() not in definitions:
                continue
            name = definitions[token.start()]
            assembled.add(name)
            if not name.endswith(("_nid_no_patch", "_nid_no_patch_cut")) or name in exported:
                done.add(name)
                live.add(name)

    def load_headers():
        loaded = []
        for header_path in _scan_library_files(path, (".h", ".hpp")):
            with open(header_path, encoding="utf-8", errors="ignore") as handle:
                loaded.append(code(handle.read()))
        return loaded

    for text, (helpers, definitions) in zip(texts, reaching_definitions(texts, load_headers, assembled)):
        if not definitions:
            continue
        wrappers = None
        linkage = None
        for definition in definitions:
            name = definition.name
            if name.endswith(("_nid_no_patch", "_nid_no_patch_cut")) and name not in exported:
                continue
            if name not in exported:
                if linkage is None:
                    linkage = Linkage(text)
                if linkage.internal(name, definition.start):
                    continue
            if definition.reaches:
                todo.add(name)
                continue
            done.add(name)
            if wrappers is None:
                wrappers = inert_wrappers(helpers)
            (inert if inert_body(definition.body, wrappers, definition.signature) else live).add(name)
    todo -= done
    inert -= live
    group = {"name": path.name, "label": path.name.removeprefix("libSce"), "done": len(done), "todo": len(todo),
             "done_names": sorted(done), "todo_names": sorted(todo), "inert_names": sorted(inert)}
    if not (done or todo):
        owners = shared_source_owners(path)
        if owners:
            group["shared_sources"] = owners[0]
    return group


def summarize(groups):
    done = sum(g["done"] for g in groups)
    total = done + sum(g["todo"] for g in groups)
    return {"done": done, "total": total, "percent": round(100 * done / total, 2) if total else 0, "groups": groups}


def collect_libraries():
    return summarize([scan_library(p) for p in sorted(PRX.iterdir()) if p.is_dir()])


def select_libraries(libraries, modules):
    if not modules:
        return libraries
    known = {group["name"] for group in libraries["groups"]}
    missing = []
    for name in modules:
        if name not in known and name not in missing:
            missing.append(name)
    if missing:
        label = "library" if len(missing) == 1 else "libraries"
        raise ValueError(f"unknown {label}: {', '.join(missing)}")
    chosen = set(modules)
    return summarize([group for group in libraries["groups"] if group["name"] in chosen])


def camel(name):
    return "".join(part.capitalize() for part in name.split("_"))


def collect_shaders():
    isa = {}
    for line in ISA.read_text(encoding="utf-8", errors="ignore").splitlines():
        if line and not line.startswith("#"):
            parts = line.split()
            if len(parts) != 2:
                raise ValueError(f"tools/rdna_isa.txt: expected 'name encoding' per line, got: {line}")
            name, encoding = parts
            isa[name] = encoding
    by_camel = {camel(name): name for name in isa}
    enum = re.search(r"enum class RdnaOpcode[^{]*\{(.*?)\};", OPCODES.read_text(encoding="utf-8", errors="ignore"), re.S)
    if not enum:
        raise ValueError(f"{OPCODES} has no enum class RdnaOpcode")
    enum = enum.group(1)
    opcodes = [o for o in re.findall(r"^\s*([A-Z]\w*)\s*[,=]", enum, re.M) if o not in OPCODE_SENTINELS]
    supported, extra = set(), []
    for opcode in opcodes:
        variants = [name for name in OPCODE_VARIANTS.get(opcode, ()) if name in isa]
        supported.update(variants)
        name = OPCODE_ALIASES.get(opcode) or by_camel.get(opcode)
        if name in isa:
            supported.add(name)
            if name.startswith("FLAT_"):
                supported.update(n for n in (s + name.removeprefix("FLAT_") for s in FLAT_SEGMENTS) if n in isa)
        elif not variants:
            extra.append(opcode)
    groups = {}
    for name, encoding in isa.items():
        group = groups.setdefault(encoding, {"name": encoding, "label": encoding, "done": 0, "todo": 0,
                                             "done_names": [], "todo_names": []})
        state = "done" if name in supported else "todo"
        group[state] += 1
        group[f"{state}_names"].append(name)
    result = summarize(sorted(groups.values(), key=lambda g: g["name"]))
    result["extra"] = extra
    return result


def collect_shader_source(root=None):
    try:
        profiles = {"gfx1013": ("LLVM gfx1013 reference", isa_profiles.load_gfx1013_forms(ISA_GFX1013))}
    except (OSError, ValueError) as error:
        raise shader_scan.ScanError(f"cannot read gfx1013 reference: {error}") from error
    return shader_scan.measure(root or ROOT, shader_scan.load_isa_forms(ISA_FORMS), profiles)


def worst_ratio(row, side):
    area = sum(row)
    return max(max(side * side * r / area ** 2, area ** 2 / (side * side * r)) for r in row)


def place(row, x, y, w, h, rects):
    thickness = sum(row) / min(w, h)
    offset = 0
    for area in row:
        length = area / thickness
        if w >= h:
            rects.append((x, y + offset, thickness, length))
        else:
            rects.append((x + offset, y, length, thickness))
        offset += length
    return (x + thickness, y, w - thickness, h) if w >= h else (x, y + thickness, w, h - thickness)


def squarify(values, x, y, w, h):
    total = sum(values)
    areas = [v * w * h / total for v in values]
    rects, row = [], []
    while areas:
        side = min(w, h)
        if not row or worst_ratio(row + [areas[0]], side) <= worst_ratio(row, side):
            row.append(areas.pop(0))
            continue
        x, y, w, h = place(row, x, y, w, h, rects)
        row = []
    if row:
        place(row, x, y, w, h, rects)
    return rects


def cells(count, x, y, w, h):
    if not count or w <= 0 or h <= 0:
        return []
    rows = max(1, min(count, round((count * h / w) ** 0.5)))
    result = []
    for r in range(rows):
        in_row = count // rows + (r < count % rows)
        cw = w / in_row
        result += [(x + c * cw, y + r * h / rows, cw, h / rows) for c in range(in_row)]
    return result


def rect(x, y, w, h, color, stroke=0.5):
    return (f'<rect x="{x:.2f}" y="{y:.2f}" width="{w:.2f}" height="{h:.2f}" '
            f'fill="{color}" stroke="{BORDER}" stroke-width="{stroke}"/>')


def text(x, y, value, size):
    return (f'<text x="{x:.2f}" y="{y:.2f}" font-family="sans-serif" font-size="{size}" fill="{TEXT}" '
            f'stroke="{BORDER}" stroke-width="3" paint-order="stroke">{escape(value)}</text>')


def tile_label(group, w, h):
    label_length = int((w - 10) / 7)
    label = group["label"][:label_length] if label_length > 0 else ""
    return label if (label == group["label"] or len(label) >= 3) and h > 22 else ""


def map_groups(data):
    return sorted((g for g in data["groups"] if g["done"] + g["todo"]), key=lambda g: -(g["done"] + g["todo"]))


def labelled_tier(groups):
    shown = list(groups)
    while len(shown) > 1:
        rects = squarify([g["done"] + g["todo"] for g in shown], 0, HEADER, PANEL_WIDTH, MAP_HEIGHT)
        if all(tile_label(g, w, h) for g, (x, y, w, h) in zip(shown, rects)):
            break
        shown.pop()
    return shown


def library_tiers(libraries):
    rest, tiers = map_groups(libraries), []
    empty = [g for g in libraries["groups"] if not g["done"] + g["todo"]]
    while rest and len(tiers) < len(LIBRARY_TIERS) - 1:
        tier = labelled_tier(rest)
        tiers.append(summarize(tier + empty))
        rest = rest[len(tier):]
    if rest:
        tiers.append(summarize(rest + empty))
    return tiers


def heading(title, data, x, y):
    return text(x + 4, y + 21, f'{title}: {data["percent"]}% ({data["done"]}/{data["total"]})', 16)


def tile(group, data, x, y, w, h):
    total = group["done"] + group["todo"]
    shared = [g["name"] for g in data["groups"] if g.get("shared_sources") == group["name"]]
    suffix = "; shared by " + ", ".join(shared) if shared else ""
    parts = [f'<g><title>{escape(group["name"])}: {group["done"]}/{total} ({100 * group["done"] / total:.0f}%){escape(suffix)}</title>']
    for i, cell in enumerate(cells(total, x + 1, y + 1, w - 2, h - 2)):
        parts.append(rect(*cell, DONE_COLOR if i < group["done"] else TODO_COLOR))
    parts.append(f'<rect x="{x:.2f}" y="{y:.2f}" width="{w:.2f}" height="{h:.2f}" fill="none" stroke="{BORDER}" stroke-width="2"/>')
    label = tile_label(group, w, h)
    if label:
        parts.append(text(x + 5, y + 16, label, 12))
    parts.append("</g>")
    return parts


def treemap(title, data, left, top=0):
    groups = map_groups(data)
    parts = [heading(title, data, left, top)]
    for group, (x, y, w, h) in zip(groups, squarify([g["done"] + g["todo"] for g in groups], left, top + HEADER, PANEL_WIDTH, MAP_HEIGHT)):
        parts += tile(group, data, x, y, w, h)
    return parts


def tile_grid(title, data, top):
    groups = sorted(map_groups(data), key=lambda g: (g["label"].lower(), g["name"]))
    width = (2 * PANEL_WIDTH + GAP) / GRID_COLUMNS
    parts = [heading(title, data, 0, top)]
    for index, group in enumerate(groups):
        parts += tile(group, data, index % GRID_COLUMNS * width, top + HEADER + index // GRID_COLUMNS * GRID_ROW, width, GRID_ROW)
    return parts, HEADER + -(-len(groups) // GRID_COLUMNS) * GRID_ROW


def shader_forms_panel(source):
    groups = []
    for form in source["forms"]:
        if form["form"] not in ("base", "vop3"):
            continue
        name = form["encoding"] if form["form"] == "base" else f'{form["encoding"]} in VOP3'
        groups.append({"name": name, "label": name, "done": form["decoded"], "todo": form["total"] - form["decoded"]})
    instructions = source["instructions"]
    return {"groups": groups, "done": instructions["decoded"], "total": instructions["total"], "percent": shader_source_percent(source)}


TRACK_COLOR, THROW_COLOR, FORMAT_COLOR = "#6e7681", "#d29922", "#2ea043"
COLUMN_WIDTH, LABEL_WIDTH, COUNT_WIDTH, ROW, NAMES_WIDTH, NAMES_LEFT = PANEL_WIDTH, 170, 64, 20, 56, 84


def form_label(form):
    return form["encoding"] if form["form"] == "base" else f'{form["encoding"]} {form["form"]}'


def still_needed(source):
    forms = source["forms"]
    plain, dpp = ("base", "vop3"), ("dpp16", "dpp8")

    def groups(kind, selected):
        return [(form_label(f), f[kind]) for f in forms if f[kind] and f["form"] in selected]

    lists = [(heading, found) for heading, found in (
        (f'Instruction forms not decoded ({sum(len(f["missing"]) for f in forms if f["form"] in plain)})', groups("missing", plain)),
        (f'DPP forms not decoded ({sum(len(f["missing"]) for f in forms if f["form"] in dpp)})', groups("missing", dpp)),
        (f'Decoded, no translator case ({sum(len(f["untranslated"]) for f in forms)})', groups("untranslated", plain + dpp)),
    ) if found] or [("Every instruction form is decoded and translated", [])]
    formats = source["formats"]
    return {
        "lists": lists,
        "throws": (source["unsupported_throws"], [(a["name"], a["count"]) for a in source["throw_areas"]]),
        "formats": [("Sampled", formats["sampled"], formats["total"]), ("Typed buffer", formats["buffer"], formats["total"]),
                    ("Vulkan host format", formats["host"], formats["total"])],
    }


def plain_text(x, y, value, size=12, anchor="start", family="sans-serif"):
    return (f'<text x="{x:.2f}" y="{y:.2f}" font-family="{family}" font-size="{size}" fill="{TEXT}" '
            f'text-anchor="{anchor}">{escape(value)}</text>')


def bar_row(x, y, label, count, of, color, caption=None):
    track = COLUMN_WIDTH - LABEL_WIDTH - COUNT_WIDTH - 8
    left = x + LABEL_WIDTH
    return [plain_text(x + 4, y + 11, label),
            f'<rect x="{left:.2f}" y="{y:.2f}" width="{track}" height="14" fill="{TRACK_COLOR}"/>',
            *([f'<rect x="{left:.2f}" y="{y:.2f}" width="{max(track * count / of, 1):.2f}" height="14" fill="{color}"/>'] if count and of else []),
            plain_text(x + COLUMN_WIDTH - 4, y + 11, caption or str(count), anchor="end")]


def still_needed_svg(source, top, left_top):
    data = still_needed(source)
    right = PANEL_WIDTH + GAP
    total, areas = data["throws"]
    parts = [text(4, left_top + 21, f"Unsupported-throw sites by area ({total})", 16),
             text(right + 4, top + 21, "Instruction forms still needed", 16)]
    y = left_top + HEADER
    for name, count in areas:
        parts += bar_row(0, y, name, count, total, THROW_COLOR)
        y += ROW
    y += 14
    parts.append(text(4, y + 4, f'Image formats (of {data["formats"][0][2]})', 16))
    y += HEADER - 10
    for label, count, of in data["formats"]:
        parts += bar_row(0, y, label, count, of, FORMAT_COLOR, f"{count}/{of}")
        y += ROW
    left_end = y
    y = top + HEADER
    for heading, groups in data["lists"]:
        parts.append(plain_text(right + 4, y + 11, heading, 13))
        y += ROW
        for label, names in groups:
            rows = textwrap.wrap(", ".join(names), NAMES_WIDTH) or [""]
            parts.append(plain_text(right + 12, y + 11, label, 11))
            for row in rows:
                parts.append(plain_text(right + NAMES_LEFT + 20, y + 11, row, 11, family="monospace"))
                y += 15
            y += 3
        y += 8
    y = max(left_end, y) + 6
    parts.append(plain_text(4, y + 10, "Still needed (counted from the code, not from playing games)", 11))
    return parts, y + 20


def render(libraries, shaders, shader_source=None):
    width, row = 2 * PANEL_WIDTH + GAP, HEADER + MAP_HEIGHT
    tiers = library_tiers(libraries)
    body = [heading("System libraries*", libraries, 0, 0)]
    top = HEADER
    for index, (title, data) in enumerate(zip(LIBRARY_TIERS[:2], tiers[:2])):
        body += treemap(title, data, index * (PANEL_WIDTH + GAP), top)
    top += row if tiers[:2] else 0
    if len(tiers) > 2:
        grid, height = tile_grid(LIBRARY_TIERS[2], tiers[2], top + 10)
        body += grid
        top += height + 10
    if shader_source:
        gfx1013 = shader_source.get("profiles", {}).get("gfx1013")
        if gfx1013:
            body += treemap("gfx1013 reference forms decoded", shader_forms_panel(gfx1013), 0, top)
            body += treemap("RDNA2 reference forms decoded", shader_forms_panel(shader_source), PANEL_WIDTH + GAP, top)
            top += row
        else:
            body += treemap("Shader forms decoded", shader_forms_panel(shader_source), 0, top)
        needed = {**shader_source, **gfx1013} if gfx1013 else shader_source
        panel, height = still_needed_svg(needed, top, top + row)
        body += panel
    else:
        body += treemap("GPU shader instructions", shaders, 0, top)
        height = top + row
    return "\n".join([f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {width} {height}" width="{width}" height="{height}">',
                      rect(0, 0, width, height, BORDER, 0), *body, "</svg>"])


def badge(label, data, value=None, color=None):
    percent = data["percent"]
    color = color or ("#4c1" if percent >= 90 else "#97ca00" if percent >= 60 else "#dfb317" if percent >= 30 else "#fe7d37")
    value = value or f"{percent}%"
    left, right = 10 + 7 * len(label), 10 + 7 * len(value)
    width = left + right
    return (f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="20" role="img" aria-label="{label}: {value}">'
            f'<rect width="{left}" height="20" fill="#555"/><rect x="{left}" width="{right}" height="20" fill="{color}"/>'
            f'<g fill="#fff" font-family="Verdana,DejaVu Sans,sans-serif" font-size="11" text-anchor="middle">'
            f'<text x="{left / 2}" y="14">{label}</text><text x="{left + right / 2}" y="14">{value}</text></g></svg>')


def table(heading, column, data):
    rows = [f"<h2>{heading}</h2>", "<table>",
            f"<tr><th>{column}</th><th>Implemented</th><th>Total</th><th>%</th></tr>"]
    for group in sorted(data["groups"], key=lambda g: g["name"].lower()):
        if "shared_sources" in group:
            owner = escape(group["shared_sources"])
            rows.append(f'<tr><td>{escape(group["name"])}</td><td colspan="3">Shared sources: '
                        f'<a href="{SOURCE}/core/libs/prx/{owner}">{owner}</a></td></tr>')
            continue
        total = group["done"] + group["todo"]
        if not total:
            rows.append(f'<tr><td>{escape(group["name"])}</td><td colspan="3">No exports</td></tr>')
            continue
        percent = f'{100 * group["done"] / total:.0f}%'
        rows.append(f'<tr><td>{escape(group["name"])}</td><td>{group["done"]}</td><td>{total}</td><td>{percent}</td></tr>')
    rows.append(f'<tr><th>Total</th><th>{data["done"]}</th><th>{data["total"]}</th><th>{data["percent"]}%</th></tr>')
    rows.append("</table>")
    return "\n".join(rows)


def shader_source_percent(source):
    instructions = source["instructions"]
    return round(100 * instructions["decoded"] / instructions["total"], 2) if instructions["total"] else 0


def shader_source_badge(source):
    instructions = source["instructions"]
    label = "RDNA2 reference" if "profiles" in source else "shader forms decoded"
    return badge(label, {"percent": shader_source_percent(source)}, f'{instructions["decoded"]}/{instructions["total"]}')


def gfx1013_section(source):
    instructions, dpp = source["instructions"], source["dpp"]
    missing = [(form_label(form), form["missing"]) for form in source["forms"] if form["missing"]]
    return "\n".join([
        "<h2>gfx1013 reference shader coverage</h2>",
        "<table><tr><th>Measure</th><th>Count</th><th>Of</th></tr>",
        f'<tr><td>Instruction forms decoded</td><td>{instructions["decoded"]}</td><td>{instructions["total"]}</td></tr>',
        f'<tr><td>Instruction forms with a translator case</td><td>{instructions["translated"]}</td><td>{instructions["total"]}</td></tr>',
        f'<tr><td>DPP forms decoded</td><td>{dpp["decoded"]}</td><td>{dpp["total"]}</td></tr></table>',
        f'<p>This pinned reference profile uses AMD RDNA1 forms, the gfx1013 BVH encodings and explicitly sourced LLVM forms '
        f'(<a href="{SOURCE}/tools/gfx1013_isa_forms.txt">inventory and provenance</a>). '
        "The PS5 uses gfx1013, but this public-source profile is not a complete console ISA or hardware verification. "
        "Literal positions, SDWA selectors and NSA operand layouts are not separate forms. "
        "Decoded means opcode-table recognition; a translator case does not establish correct execution.</p>",
        "<h3>gfx1013 reference forms not decoded</h3>",
        "<table>" + "".join(f"<tr><td>{escape(label)}</td><td class='names'><code>{escape(', '.join(names))}</code></td></tr>"
                             for label, names in missing) + "</table>" if missing else "<p>Every reference form is decoded.</p>",
    ])


def bar_rows(rows, color, scale=0):
    return [f'<tr><td>{escape(label)}</td><td><div class="bar"><i style="width:{100 * count / (scale or of) if scale or of else 0:.2f}%;background:{color}"></i></div></td>'
            f'<td class="n">{count}{f"/{of}" if of else ""}</td></tr>' for label, count, of in rows]


def still_needed_html(source):
    data = still_needed(source)
    total, areas = data["throws"]
    left = [f"<h3>Unsupported-throw sites by area ({total})</h3>", "<table>", *bar_rows([(n, c, 0) for n, c in areas], THROW_COLOR, total), "</table>",
            f'<h3>Image formats (of {data["formats"][0][2]})</h3>', "<table>", *bar_rows(data["formats"], FORMAT_COLOR), "</table>",
            f'<p>Formats are counted from <a href="{SOURCE}/core/libs/prx/libSceAgcDriver/Graphics/src/TextureFormat.cpp">TextureFormat.cpp</a> '
            "and the recompiler's format tables: sampled and typed-buffer formats the recompiler handles, and formats the driver maps to a Vulkan format.</p>"]
    right = ["<h3>RDNA2 reference forms still needed</h3>" if "profiles" in source else "<h3>Instruction forms still needed</h3>"]
    for heading, groups in data["lists"]:
        right.append(f"<h4>{escape(heading)}</h4>")
        if groups:
            right.append("<table>" + "".join(f"<tr><td>{escape(label)}</td><td class='names'><code>{escape(', '.join(names))}</code></td></tr>" for label, names in groups) + "</table>")
    title = "<h2>RDNA2 reference still needed</h2>" if "profiles" in source else "<h2>Still needed</h2>"
    return "\n".join([title, '<div class="cols">', "<div>", *left, "</div>", "<div>", *right, "</div>", "</div>"])


def shader_source_section(source):
    if not source:
        return ""
    instructions, dpp = source["instructions"], source["dpp"]
    return "\n".join([
        "<h2>RDNA2 reference: counted from the code</h2>" if "profiles" in source else "<h2>Shader recompiler: counted from the code</h2>",
        "<table>",
        "<tr><th>Measure</th><th>Count</th><th>Of</th></tr>",
        f'<tr><td>Instruction forms decoded</td><td>{instructions["decoded"]}</td><td>{instructions["total"]}</td></tr>',
        f'<tr><td>Instruction forms with a translator case</td><td>{instructions["translated"]}</td><td>{instructions["total"]}</td></tr>',
        f'<tr><td>DPP forms decoded</td><td>{dpp["decoded"]}</td><td>{dpp["total"]}</td></tr>',
        f'<tr><td>Unsupported-throw sites in the recompiler</td><td>{source["unsupported_throws"]}</td><td></td></tr>',
        "</table>",
        "<table>",
        "<tr><th>Encoding</th><th>Form</th><th>Decoded</th><th>Translated</th><th>Total</th></tr>",
        *[f'<tr><td>{escape(f["encoding"])}</td><td>{escape(f["form"])}</td><td>{f["decoded"]}</td><td>{f["translated"]}</td><td>{f["total"]}</td></tr>'
          for f in source["forms"]],
        "</table>",
        "<p>These numbers are counted from the recompiler's code, not from playing games, so they say nothing about whether a game runs. A form counts as decoded when the decoder's opcode tables map it "
        "to an opcode (VOP3 and DPP rules included); it counts as translated when the translator has a case for that opcode, "
        "which does not show the translation is correct. Forms are the base, VOP3 and DPP encodings of the AMD RDNA 2 "
        f'instruction set (<a href="{SOURCE}/tools/rdna2_isa_forms.txt">tools/rdna2_isa_forms.txt</a>); SDWA forms are not counted. '
        f'Unsupported throws are <code>throw</code> statements in <a href="{SOURCE}/core/shader/recompiler">core/shader/recompiler</a> '
        "whose message or exception type says unsupported or not implemented. "
        f'<a href="{SOURCE}/tools/shader_scan.py">How it is measured</a>.</p>',
        still_needed_html(source),
    ])


def shader_page(shader_source, shaders, extra):
    if shader_source:
        profile = shader_source.get("profiles", {}).get("gfx1013")
        return ([gfx1013_section(profile)] if profile else []) + [shader_source_section(shader_source)]
    return [table("GPU shader instructions", "Encoding", shaders),
            f'<p>The total is the AMD RDNA 1 + RDNA 2 instruction list (<a href="{SOURCE}/tools/rdna_isa.txt">tools/rdna_isa.txt</a>). '
            f'An instruction is implemented when the <a href="{SOURCE}/core/shader/recompiler/RdnaDecoder">decoder</a> recognizes it. '
            f"Decoded opcodes not present in AMD's public list are not counted: {extra}.</p>"]


def open_pr_progress_section(estimate):
    if not estimate:
        return []
    generated = escape(estimate["generated_at"])
    if not estimate["available"]:
        return ["<h2>Potential library progress with open PRs</h2>",
                f'<p>Estimate unavailable as of <time>{generated}</time>: {escape(estimate["reason"])}. '
                "The current progress above is unchanged.</p>"]
    baseline, potential = estimate["baseline"], estimate["potential"]
    prs = estimate["pull_requests"]
    reasons = estimate.get("excluded_reasons", {})
    reason_text = ", ".join(f"{count} {escape(reason.replace('_', ' '))}" for reason, count in sorted(reasons.items()))
    excluded = f'; {prs["excluded"]} excluded' + (f" ({reason_text})" if reason_text else "")
    return ["<h2>Potential library progress with open PRs</h2>",
            f'<p>Current: {baseline["percent"]}% ({baseline["done"]}/{baseline["total"]}); '
            f'combined open PR estimate: <strong>{potential["percent"]}% '
            f'({potential["done"]}/{potential["total"]})</strong>. '
            f'{prs["included"]} of {prs["open"]} open PRs included{excluded}. '
            f'Generated <time>{generated}</time>. This is a source-count estimate, not a compatibility measure.</p>']


def summary(libraries, shaders, shader_source=None, open_pr_progress=None):
    extra = ", ".join(f"<code>{escape(o)}</code>" for o in shaders["extra"])
    return "\n".join([
        '<!DOCTYPE html>',
        '<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1">',
        '<title>AnyPS5 progress</title>',
        '<style>body{font-family:sans-serif;max-width:1000px;margin:auto;padding:16px}img{max-width:100%}'
        '.bar{background:#6e7681;width:100%;min-width:120px;height:14px}.bar i{display:block;height:14px}td.n{text-align:right}td.names{text-align:left}.cols{display:grid;grid-template-columns:minmax(0,1fr) minmax(0,1fr);gap:24px}.cols table{width:100%}@media(max-width:760px){.cols{grid-template-columns:1fr}}table{border-collapse:collapse}th,td{border:1px solid #ccc;padding:2px 8px}td+td,th+th{text-align:right}</style>',
        '</head><body>',
        "<h1>Progress</h1>",
        '<img src="progress.svg" alt="progress">',
        f'<p>Generated by <a href="{SOURCE}/tools/progress.py">tools/progress.py</a> after relevant pushes to <code>main</code>, '
        "daily, and when the Pages workflow is run manually. "
        'Raw numbers: <a href="progress.json">progress.json</a>.</p>',
        table("System libraries", "Library", libraries),
        "<p>A function is implemented when it no longer calls <code>NotImplemented_nid_no_patch</code>; "
        "comparison reports separately mark implemented functions whose body only returns a constant or throws. "
        f'The total only includes functions already declared in <a href="{SOURCE}/core/libs/prx">core/libs/prx</a>, '
        "not every function exported by the PS5 firmware. Shared-source libraries are counted only at their source library. "
        "Libraries with no exports only provide an empty module for titles that load them. "
        f'<a href="{SOURCE}/docs/dev/PROGRESS.md">Counting rules</a>.</p>',
        *open_pr_progress_section(open_pr_progress),
        *shader_page(shader_source, shaders, extra),
        '<p><a href="history.html">Progress history</a> (one point per day).</p>',
        "</body></html>",
    ]) + "\n"


def names(data, state, owners=None):
    owners = owners or {}
    return {(owners.get(g["name"], g["name"]), n) for g in data["groups"] for n in g.get(f"{state}_names", [])}


def details(icon, title, column, items):
    if not items:
        return []
    rows = [f"<details>\n<summary>{icon} {len(items)} {title}</summary>\n", f"| {column} | Name |", "| - | - |"]
    rows += [f"| {group} | `{name}` |" for group, name in sorted(items)[:REPORT_ROWS]]
    if len(items) > REPORT_ROWS:
        rows.append(f"| ... | {len(items) - REPORT_ROWS} more |")
    return rows + ["\n</details>"]


def exact_delta(base, head):
    try:
        return Fraction(100 * head["done"], head["total"]) - Fraction(100 * base["done"], base["total"])
    except (KeyError, TypeError, ZeroDivisionError):
        return None


def small_delta(delta):
    text = f"{float(delta):+.4f}"
    if float(text) == 0:
        return f'{"+" if delta > 0 else "-"}<0.0001'
    return text.rstrip("0")


def compare(title, column, unit, base, head):
    owners = {g["name"]: g["shared_sources"] for g in head["groups"] if "shared_sources" in g}
    previous = {g["name"]: g.get("shared_sources") for g in base["groups"]}
    shared = {(name, owner) for name, owner in owners.items() if previous.get(name) != owner}
    base_done, head_done = names(base, "done", owners), names(head, "done", owners)
    base_all, head_all = base_done | names(base, "todo", owners), head_done | names(head, "todo", owners)
    implemented, declared = head_done - base_done, head_all - base_all - head_done
    regressed, removed = base_done & (head_all - head_done), base_all - head_all
    inert = implemented & names(head, "inert", owners)
    if not (implemented or declared or regressed or removed or shared):
        return []
    delta = round(head["percent"] - base["percent"], 2)
    exact = exact_delta(base, head) if delta == 0 else None
    shown = small_delta(exact) if exact else f"{delta:+}"
    icon = "📈" if (exact or delta) > 0 else "📉" if (exact or delta) < 0 else "➖"
    counts = [f"{n:+} {label}" for n, label in ((len(implemented), "implemented"), (len(declared), "declared"),
                                                (-len(regressed), "reverted"), (-len(removed), "removed")) if n]
    unit = unit[:-1] if len(implemented) + len(declared) + len(regressed) + len(removed) == 1 else unit
    changes =f', {", ".join(counts)} {unit}' if counts else ""
    lines = [f'{icon} **{title}**: {head["percent"]}% ({shown}%{changes})', ""]
    lines += details("✅", "implemented", column, implemented)
    lines += details("⚪", "behaviourally empty (constant return or throw)", column, inert)
    lines += details("🆕", "declared as stubs", column, declared)
    lines += details("⚠️", "went back to stubs", column, regressed)
    lines += details("🗑️", "removed", column, removed)
    lines += details("🔗", "now sharing sources", column, shared)
    return lines + [""]


def library_progress(group):
    if group is None:
        return "Not present", None
    if "shared_sources" in group:
        return f'Shared sources: `{group["shared_sources"]}`', None
    if "done" not in group or "todo" not in group:
        return "Counts unavailable", None
    total = group["done"] + group["todo"]
    if not total:
        return "No exports", None
    percent = 100 * group["done"] / total
    return f'{group["done"]}/{total} ({percent:.2f}%)', percent


def percent_change(old_percent, new_percent):
    if old_percent is None or new_percent is None:
        return "n/a"
    return f'{round(new_percent - old_percent, 2) or 0:+.2f}%'


def library_report(base, head):
    before, after = ({group["name"]: group for group in data["groups"]} for data in (base, head))
    fields = ("done", "todo", "done_names", "todo_names", "shared_sources")
    rows = []
    for name in sorted(before.keys() | after.keys(), key=str.casefold):
        old, new = before.get(name), after.get(name)
        if old is not None and new is not None and all(old.get(field) == new.get(field) for field in fields):
            continue
        old_text, old_percent = library_progress(old)
        new_text, new_percent = library_progress(new)
        delta = percent_change(old_percent, new_percent)
        if delta == "+0.00%":
            exact = exact_delta({"done": old["done"], "total": old["done"] + old["todo"]},
                                {"done": new["done"], "total": new["done"] + new["todo"]})
            if exact:
                delta = f"{small_delta(exact)}%"
        rows.append(f"| {name} | {old_text} | {new_text} | {delta} |")
    if not rows:
        return []
    lines = ["#### Changed libraries (implemented exports)", "",
             "| Library | Before | After | Change |", "| - | - | - | - |"] + rows[:REPORT_ROWS]
    if len(rows) > REPORT_ROWS:
        lines.append(f"| ... | {len(rows) - REPORT_ROWS} more libraries | | |")
    return lines + [""]


def report(base, head):
    lines = compare("System libraries", "Library", "functions", base["libraries"], head["libraries"])
    lines += library_report(base["libraries"], head["libraries"])
    lines += compare("GPU shader instructions", "Encoding", "instructions", base["shaders"], head["shaders"])
    before = base.get("shader_source", {}).get("profiles", {}).get("gfx1013")
    after = head.get("shader_source", {}).get("profiles", {}).get("gfx1013")
    if after and (not before or before["instructions"] != after["instructions"] or before["dpp"] != after["dpp"]):
        instructions = after["instructions"]
        previous = f'{before["instructions"]["decoded"]}/{before["instructions"]["total"]}' if before else "Not measured"
        lines += ["#### gfx1013 reference shader forms", "", "| Before | After |", "| - | - |",
                  f'| {previous} | {instructions["decoded"]}/{instructions["total"]} ({shader_source_percent(after)}%) |', ""]
    return "\n".join(lines)


SITE_REPORTS = ("progress.json", "badge-libraries.svg", "badge-shaders.svg", "badge-shaders-gfx1013.svg", "progress.svg", "progress.html")


def require_distinct_reports(output):
    paths = [output / name for name in SITE_REPORTS]
    for index, path in enumerate(paths):
        if not path.exists():
            continue
        if not path.is_file():
            raise ValueError(f"{path}: report destination must be a regular file")
        for other in paths[index + 1:]:
            if other.exists() and path.samefile(other):
                raise ValueError(f"{path.name} and {other.name} in {output} are the same file")
    targets = [os.path.normcase(os.path.realpath(path)) for path in paths]
    for index, target in enumerate(targets):
        for other in range(index + 1, len(targets)):
            if targets[other] == target:
                raise ValueError(f"{paths[index].name} and {paths[other].name} in {output} are the same file")


def site_data(libraries, shaders, shader_source, open_pr_progress=None):
    data = {"libraries": libraries, "shaders": shaders}
    if shader_source:
        data["shader_source"] = {key: value for key, value in shader_source.items() if key != "untranslated"}
    if open_pr_progress:
        data["open_pr_progress"] = open_pr_progress
    return data


def write_site(output, libraries, shaders, shader_source=None, open_pr_progress=None):
    output.mkdir(parents=True, exist_ok=True)
    require_distinct_reports(output)
    (output / "progress.json").write_text(json.dumps(site_data(libraries, shaders, shader_source, open_pr_progress), indent=2), encoding="utf-8")
    (output / "badge-libraries.svg").write_text(badge("libraries*", libraries), encoding="utf-8")
    (output / "badge-shaders.svg").write_text(shader_source_badge(shader_source) if shader_source else badge("shaders", shaders), encoding="utf-8")
    profile = shader_source.get("profiles", {}).get("gfx1013") if shader_source else None
    if profile:
        instructions = profile["instructions"]
        (output / "badge-shaders-gfx1013.svg").write_text(
            badge("gfx1013 reference", {"percent": shader_source_percent(profile)}, f'{instructions["decoded"]}/{instructions["total"]}'), encoding="utf-8")
    else:
        (output / "badge-shaders-gfx1013.svg").write_text(badge("gfx1013 reference", {"percent": 0}, "unavailable", TRACK_COLOR), encoding="utf-8")
    (output / "progress.svg").write_text(render(libraries, shaders, shader_source), encoding="utf-8")
    (output / "progress.html").write_text(summary(libraries, shaders, shader_source, open_pr_progress), encoding="utf-8")


def validate_open_pr_progress(estimate):
    if not isinstance(estimate, dict) or type(estimate.get("available")) is not bool:
        raise ValueError("open_pr_progress must be an object with a boolean available field")
    if not isinstance(estimate.get("generated_at"), str):
        raise ValueError("open_pr_progress.generated_at must be a string")
    if not estimate["available"]:
        if not isinstance(estimate.get("reason"), str):
            raise ValueError("unavailable open_pr_progress must include a reason")
        return
    for name in ("baseline", "potential"):
        data = estimate.get(name)
        if not isinstance(data, dict):
            raise ValueError(f"open_pr_progress.{name} must be an object")
        for field in ("done", "total"):
            if type(data.get(field)) is not int or data[field] < 0:
                raise ValueError(f"open_pr_progress.{name}.{field} must be a non-negative integer")
        if data["done"] > data["total"]:
            raise ValueError(f"open_pr_progress.{name}.done must not exceed total")
        if isinstance(data.get("percent"), bool) or not isinstance(data.get("percent"), (int, float)) or not 0 <= data["percent"] <= 100:
            raise ValueError(f"open_pr_progress.{name}.percent must be between 0 and 100")
        expected = round(100 * data["done"] / data["total"], 2) if data["total"] else 0
        if data["percent"] != expected:
            raise ValueError(f"open_pr_progress.{name}.percent does not match its counts")
    prs = estimate.get("pull_requests")
    if not isinstance(prs, dict):
        raise ValueError("open_pr_progress.pull_requests must be an object")
    for field in ("open", "included", "excluded"):
        if type(prs.get(field)) is not int or prs[field] < 0:
            raise ValueError(f"open_pr_progress.pull_requests.{field} must be a non-negative integer")
    if prs["included"] + prs["excluded"] != prs["open"]:
        raise ValueError("open_pr_progress included and excluded counts must match the open count")
    reasons = estimate.get("excluded_reasons", {})
    if not isinstance(reasons, dict) or any(not isinstance(key, str) or type(value) is not int or value < 0
                                            for key, value in reasons.items()):
        raise ValueError("open_pr_progress.excluded_reasons must map strings to non-negative integers")
    if sum(reasons.values()) != prs["excluded"]:
        raise ValueError("open_pr_progress excluded reason counts must match the excluded count")


def validate_snapshot(snapshot):
    if not isinstance(snapshot, dict):
        raise ValueError("snapshot must be an object")
    source = snapshot.get("shader_source", {})
    if not isinstance(source, dict):
        raise ValueError("shader_source must be an object")
    profiles = source.get("profiles", {})
    if not isinstance(profiles, dict):
        raise ValueError("shader_source.profiles must be an object")
    profile = profiles.get("gfx1013")
    if profile is not None:
        if not isinstance(profile, dict):
            raise ValueError("gfx1013 profile must be an object")
        for field, keys in (("instructions", ("total", "decoded", "translated")), ("dpp", ("total", "decoded"))):
            counts = profile.get(field)
            if not isinstance(counts, dict) or any(type(counts.get(key)) is not int or counts[key] < 0 for key in keys):
                raise ValueError(f"gfx1013.{field} must contain non-negative integer counts")
            if any(counts[key] > counts["total"] for key in keys[1:]):
                raise ValueError(f"gfx1013.{field} count exceeds its total")
    for section in ("libraries", "shaders"):
        data = snapshot.get(section)
        if not isinstance(data, dict):
            raise ValueError(f"{section} must be an object")
        groups = data.get("groups")
        if not isinstance(groups, list):
            raise ValueError(f"{section}.groups must be a list")
        percent = data.get("percent")
        if isinstance(percent, bool) or not isinstance(percent, (int, float)) or not 0 <= percent <= 100:
            raise ValueError(f"{section}.percent must be a finite number between 0 and 100")
        for field in ("done", "total"):
            if field in data and (type(data[field]) is not int or data[field] < 0):
                raise ValueError(f"{section}.{field} must be a non-negative integer")
        if "done" in data and "total" in data and data["done"] > data["total"]:
            raise ValueError(f"{section}.done must not exceed total")
        seen = set()
        for index, group in enumerate(groups):
            location = f"{section}.groups[{index}]"
            if not isinstance(group, dict) or not isinstance(group.get("name"), str):
                raise ValueError(f"{location} must be an object with a string name")
            if group["name"] in seen:
                raise ValueError(f"{location}.name duplicates {group['name']!r}")
            seen.add(group["name"])
            if "shared_sources" in group and not isinstance(group["shared_sources"], str):
                raise ValueError(f"{location}.shared_sources must be a string")
            if section == "libraries":
                for field in ("done", "todo"):
                    if field in group and (type(group[field]) is not int or group[field] < 0):
                        raise ValueError(f"{location}.{field} must be a non-negative integer")
            symbols = {}
            for field in ("done_names", "todo_names", "inert_names"):
                values = group.get(field, [])
                if not isinstance(values, list) or any(not isinstance(value, str) for value in values):
                    raise ValueError(f"{location}.{field} must be a list of strings")
                symbols[field] = set(values)
                if len(symbols[field]) != len(values):
                    raise ValueError(f"{location}.{field} contains duplicate symbols")
            if symbols["done_names"] & symbols["todo_names"]:
                raise ValueError(f"{location} lists symbols as both done and todo")
            if "done_names" in group and not symbols["inert_names"] <= symbols["done_names"]:
                raise ValueError(f"{location}.inert_names must be a subset of done_names")


def regenerate_isa_forms(xml):
    raw = xml.read_bytes()
    text = raw.decode("utf-8")
    try:
        metadata = ET.fromstring(text)
    except ET.ParseError as error:
        raise ValueError(f"{xml}: invalid ISA XML: {error}") from error
    if metadata.findtext("ISA/Architecture/ArchitectureName") != "AMD RDNA 2":
        raise ValueError(f"{xml}: expected AMD RDNA 2 XML")
    if metadata.findtext("Document/License") != "MIT":
        raise ValueError(f"{xml}: expected MIT-licensed ISA XML")
    copyright = metadata.findtext("Document/Copyright")
    release = metadata.findtext("Document/ReleaseDate")
    if not copyright or not release:
        raise ValueError(f"{xml}: missing ISA copyright or release date")
    original = [line for line in ISA_FORMS.read_text(encoding="utf-8").splitlines() if line.startswith("#")]
    license_start = next((i for i, line in enumerate(original) if line.startswith("# Permission is hereby granted")), None)
    if license_start is None:
        raise ValueError(f"{ISA_FORMS}: missing MIT permission notice")
    header = ["# RDNA 2 instruction forms: name encoding form opcode",
              f"# Derived from AMD's machine-readable ISA ({xml.name}, release {release}, sha256 of the XML",
              f"# {hashlib.sha256(raw).hexdigest()}). Regenerate with:",
              "#   python3 tools/progress.py --regenerate-isa-forms amdgpu_isa_rdna2.xml", "#",
              f"# {copyright} The XML is published under the MIT licence:", "#", *original[license_start:]]
    forms = shader_scan.parse_isa_xml(text)
    if not forms:
        raise ValueError(f"{xml}: ISA XML has no recognized instruction forms")
    ISA_FORMS.write_text("\n".join(header + [" ".join(map(str, form)) for form in forms]) + "\n", encoding="utf-8")
    print(f"{ISA_FORMS}: {len(forms)} forms")
    return 0


def main(argv=None):
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8", errors="backslashreplace")
    if hasattr(sys.stderr, "reconfigure"):
        sys.stderr.reconfigure(encoding="utf-8", errors="backslashreplace")
    global PRX, OPCODES
    previous = PRX, OPCODES
    parser = argparse.ArgumentParser()
    parser.add_argument("output", type=Path, nargs="?")
    parser.add_argument("--root", type=Path, help="source tree to measure instead of the one containing this script")
    parser.add_argument("--compare", type=Path, nargs=2, metavar=("BASE", "HEAD"),
                        help="print a markdown report of the changes between two progress.json files")
    parser.add_argument("--module", action="append", metavar="NAME",
                        help="limit library counts to these core/libs/prx directories; repeat to include several. Shader counts stay complete")
    parser.add_argument("--require-shader-source", action="store_true",
                        help="fail when the shader source scan cannot read the recompiler, instead of leaving its numbers out (the Pages workflow uses this)")
    parser.add_argument("--open-pr-estimate", type=Path,
                        help="include a JSON estimate from tools/progress_prs.py in progress.json and progress.html")
    parser.add_argument("--regenerate-isa-forms", type=Path, metavar="XML",
                        help="rewrite tools/rdna2_isa_forms.txt from AMD's amdgpu_isa_rdna2.xml (machine-readable ISA, MIT) and exit")
    parser.add_argument("--json", action="store_true",
                        help="print the progress.json object to stdout instead of the summary lines")
    parser.add_argument("--regenerate-gfx1013-forms", type=Path, nargs=2, metavar=("RDNA1_XML", "RDNA2_XML"),
                        help="rewrite the pinned LLVM gfx1013 reference inventory from AMD's RDNA1 and RDNA2 XML and exit")
    try:
        args = parser.parse_args(argv)
        if args.regenerate_isa_forms and args.regenerate_gfx1013_forms:
            parser.error("--regenerate-isa-forms does not combine with --regenerate-gfx1013-forms")
        if args.compare:
            if args.module or args.json:
                parser.error("--compare does not combine with --module or --json")
            snapshots = []
            for path in args.compare:
                try:
                    snapshots.append(json.loads(path.read_text(encoding="utf-8-sig"), object_pairs_hook=unique_json_keys))
                except (OSError, ValueError, RecursionError) as error:
                    parser.error(f"{path}: {error}")
            for path, snapshot in zip(args.compare, snapshots):
                try:
                    validate_snapshot(snapshot)
                except ValueError as error:
                    parser.error(f"{path}: {error}")
            base, head = snapshots
            print(report(base, head), end="")
            return 0
        if args.regenerate_isa_forms or args.regenerate_gfx1013_forms:
            try:
                if args.regenerate_isa_forms:
                    return regenerate_isa_forms(args.regenerate_isa_forms)
                isa_profiles.write_gfx1013_forms(ISA_GFX1013, *(path.read_text(encoding="utf-8") for path in args.regenerate_gfx1013_forms))
                return 0
            except (OSError, ValueError) as error:
                parser.error(str(error))
        if not args.output and not (args.module or args.json):
            parser.error("the output directory is required")
        if args.root:
            PRX = args.root / "core" / "libs" / "prx"
            OPCODES = args.root / previous[1].relative_to(ROOT)
        open_pr_progress = None
        if args.open_pr_estimate:
            try:
                open_pr_progress = json.loads(args.open_pr_estimate.read_text(encoding="utf-8-sig"), object_pairs_hook=unique_json_keys)
                validate_open_pr_progress(open_pr_progress)
            except (OSError, ValueError, RecursionError) as error:
                parser.error(f"{args.open_pr_estimate}: {error}")
        try:
            libraries, shaders = collect_libraries(), collect_shaders()
        except FileNotFoundError as error:
            parser.error(f"{args.root or ROOT} is not a source tree: {error.filename} not found")
        except OSError as error:
            parser.error(f"{args.root or ROOT} is not a source tree: {error.filename}: {error.strerror}")
        except ValueError as error:
            parser.error(f"{args.root or ROOT} is not a source tree: {error}")
        try:
            shader_source = collect_shader_source(args.root)
        except shader_scan.ScanError as error:
            if args.require_shader_source:
                parser.error(str(error))
            print(f"warning: {error}; the shader source numbers are left out", file=sys.stderr)
            shader_source = None
        try:
            libraries = select_libraries(libraries, args.module or [])
        except ValueError as error:
            parser.error(str(error))
        if args.output:
            try:
                write_site(args.output, libraries, shaders, shader_source, open_pr_progress)
            except ValueError as error:
                parser.error(str(error))
            except OSError as error:
                parser.error(f"{args.output}: {error.strerror or error}")
        if args.json:
            print(json.dumps(site_data(libraries, shaders, shader_source, open_pr_progress), indent=2))
        else:
            print(f'libraries {libraries["done"]}/{libraries["total"]} ({libraries["percent"]}%)')
            print(f'shader opcode names declared {shaders["done"]}/{shaders["total"]} ({shaders["percent"]}%)')
            if shader_source:
                instructions = shader_source["instructions"]
                print(f'shader forms decoded {instructions["decoded"]}/{instructions["total"]} ({shader_source_percent(shader_source)}%), '
                      f'translated {instructions["translated"]}, unsupported throws {shader_source["unsupported_throws"]}')
                profile = shader_source.get("profiles", {}).get("gfx1013")
                if profile:
                    instructions = profile["instructions"]
                    print(f'gfx1013 reference forms decoded {instructions["decoded"]}/{instructions["total"]} '
                          f'({shader_source_percent(profile)}%), translated {instructions["translated"]}')
        return 0
    finally:
        PRX, OPCODES = previous


if __name__ == "__main__":
    raise SystemExit(main())
