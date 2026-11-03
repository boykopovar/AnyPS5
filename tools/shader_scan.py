import re
from pathlib import Path

RECOMPILER = "core/shader/recompiler/"
DECODER = f"{RECOMPILER}RdnaDecoder/src/"
AGC_DRIVER = "core/libs/prx/libSceAgcDriver/"
TEXTURE_FORMATS = f"{AGC_DRIVER}Graphics/src/TextureFormat.cpp"
SCANNED_DIRECTORIES = (RECOMPILER.rstrip("/"), AGC_DRIVER.rstrip("/"))
FORMS = ("base", "vop3", "dpp16", "dpp8")
TEST_PATH = re.compile(r"/tests?/")


class ScanError(Exception):
    def __init__(self, what):
        super().__init__(f"shader scan: {what}. The recompiler source may have been restructured; update tools/shader_scan.py.")


def skip_literal(source, start):
    quote = source[start]
    j = start + 1
    while j < len(source) and source[j] != quote and source[j] != "\n":
        j += 2 if source[j] == "\\" else 1
    return j


def strip_comments(source):
    out, i, n = [], 0, len(source)
    while i < n:
        c, nxt = source[i], source[i + 1] if i + 1 < n else ""
        if c == "/" and nxt == "/":
            end = source.find("\n", i)
            stop = n if end < 0 else end
            out.append(" " * (stop - i))
            i = stop
        elif c == "/" and nxt == "*":
            end = source.find("*/", i + 2)
            stop = n if end < 0 else end + 2
            out.append(re.sub(r"[^\n]", " ", source[i:stop]))
            i = stop
        elif c in "\"'":
            j = skip_literal(source, i)
            out.append(source[i:j + 1])
            i = j + 1
        else:
            out.append(c)
            i += 1
    return "".join(out)


def body_after(source, header):
    match = header.search(source)
    if not match:
        return None
    opening = source.find("{", match.end() - 1)
    if opening < 0:
        return None
    depth, i = 0, opening
    while i < len(source):
        c = source[i]
        if c in "\"'":
            i = skip_literal(source, i)
        elif c == "{":
            depth += 1
        elif c == "}":
            depth -= 1
            if depth == 0:
                return source[opening + 1:i]
        i += 1
    return None


def function_body(source, name):
    return body_after(source, re.compile(rf"\b{name}\s*\([^)]*\)\s*(?:const\s*)?\{{"))


def array_body(source, name):
    return body_after(source, re.compile(rf"\b{name}\s*(?:\[[^\]]*\])?\s*=?\s*\{{"))


def case_names(body, kind):
    return set(re.findall(rf"case\s+{kind}::(\w+)\s*:", body))


def need(value, what):
    if value is None:
        raise ScanError(f"no {what}")
    return value


def joined(files, prefix):
    return "\n".join(strip_comments(text) for path, text in files.items()
                     if path.startswith(prefix) and not TEST_PATH.search(path))


def search(files, what, extract):
    for path, raw in files.items():
        if TEST_PATH.search(path):
            continue
        found = extract(strip_comments(raw))
        if found is not None:
            return found
    raise ScanError(f"no {what}")


VECTOR = {"VOP1", "VOP2", "VOPC"}


def classify(tag, native):
    if tag in ("ENC_VOP3", "VOP3_SDST_ENC"):
        return (native, "vop3") if native in VECTOR else ("VOP3", "base")
    if tag == "ENC_FLAT_GLBL":
        return "GLOBAL", "base"
    if tag == "ENC_FLAT_SCRATCH":
        return "SCRATCH", "base"
    own = re.fullmatch(r"ENC_(\w+)", tag)
    if own:
        return own.group(1), "base"
    modified = re.fullmatch(r"(VOP1|VOP2|VOPC)_VOP_(DPP16|DPP8)", tag)
    if modified:
        return modified.group(1), modified.group(2).lower()
    return None


def parse_isa_xml(xml):
    start = xml.find("<Instructions>")
    if start < 0:
        raise ScanError("the ISA XML has no <Instructions>")
    forms = []
    for instruction in re.finditer(r"<InstructionName>(\w+)</InstructionName>([\s\S]*?)</InstructionEncodings>", xml[start:]):
        name = instruction.group(1)
        encodings = [(m.group(1), int(m.group(2))) for m in
                     re.finditer(r'<EncodingName>(\w+)</EncodingName>[\s\S]*?<Opcode Radix="10">(\d+)</Opcode>', instruction.group(2))]
        native = next((m.group(1) for tag, _ in encodings
                       if (m := re.fullmatch(r"ENC_(\w+)", tag)) and m.group(1) != "VOP3"), None)
        for tag, opcode in encodings:
            kind = classify(tag, native)
            if kind:
                forms.append((name, kind[0], kind[1], opcode))
    return forms


def load_isa_forms(path):
    forms = []
    for line in Path(path).read_text(encoding="utf-8").splitlines():
        if line and not line.startswith("#"):
            name, encoding, form, opcode = line.split()
            forms.append((name, encoding, form, int(opcode)))
    return forms


TABLE_KEYS = ("vop3encodedvop1", "vop3p", "vop3", "vopc", "vop2", "vop1", "vintrp", "sop1", "sop2", "sopc", "sopk", "sopp",
              "smem", "mubuf", "mtbuf", "flat", "ds", "image")
TABLE_ENCODING = {"vop3encodedvop1": "VOP3:VOP1", "image": "MIMG"}
BLOCK = re.compile(r"^(?![\s}#])(?:[\w:<>&*,]+[ \t]+)*?(\w+)\s*(?:\[\]\s*=|\()", re.M)
PAIR = re.compile(r"(?:\{\s*|case\s+)0x([0-9a-fA-F]+)u?\s*(?:,|:\s*return)\s*RdnaOpcode::(\w+)")
CASE_RUN = re.compile(r"((?:\bcase\s+0x[0-9a-fA-F]+u?\s*:\s*)+)return\s+RdnaOpcode::(\w+)")
CASE_NUMBER = re.compile(r"0x([0-9a-fA-F]+)u?")
REQUIRED_TABLES = ("SOP1", "SOP2", "SOPC", "SOPK", "SOPP", "VOP1", "VOP2", "VOPC", "VOP3", "VOP3:VOP1", "VOP3P",
                   "SMEM", "MUBUF", "MTBUF", "FLAT", "DS", "MIMG")


class Decoder:
    def __init__(self):
        self.tables = {}
        self.loose = {}
        self.vop3_vop2_refused = set()
        self.families = set()
        self.no_dpp = set()
        self.no_dpp8 = set()


def scan_decoder(files):
    d = Decoder()
    for path, raw in files.items():
        if not path.startswith(DECODER) or not path.endswith(".cpp"):
            continue
        text = strip_comments(raw)
        starts = [(m.start(), m.group(1)) for m in BLOCK.finditer(text)] + [(len(text), "")]
        for (at, name), (stop, _) in zip(starts, starts[1:]):
            block = text[at:stop]
            key = re.sub(r"^(decode|lookup)", "", name.lower())
            table = next((k for k in TABLE_KEYS if key.startswith(k)), None)
            if table:
                entries = d.tables.setdefault(TABLE_ENCODING.get(table, table.upper()), {})
                for m in CASE_RUN.finditer(block):
                    for number in CASE_NUMBER.finditer(m.group(1)):
                        entries[int(number.group(1), 16)] = m.group(2)
                for m in PAIR.finditer(block):
                    entries[int(m.group(1), 16)] = m.group(2)
            else:
                numbers = [int(m.group(1), 16) for m in re.finditer(r"\b0x([0-9a-fA-F]+)u?\b", block)]
                for opcode in re.findall(r"RdnaOpcode::(\w+)", block):
                    d.loose.setdefault(opcode, set()).update(numbers)
    d.tables = {k: v for k, v in d.tables.items() if v}
    for encoding in REQUIRED_TABLES:
        if not d.tables.get(encoding):
            raise ScanError(f"no {encoding} opcode table")

    def required(name):
        return search(files, f"{name}()", lambda text: function_body(text, name))

    for name in ("isUnsupportedVop3EncodedVop2Alias", "isVop2LiteralMadOpcode"):
        d.vop3_vop2_refused.update(int(m, 16) for m in re.findall(r"0x([0-9a-fA-F]+)u?", required(name)))
    dispatch = joined(files, DECODER)
    d.families = set(re.findall(r"case\s+RdnaInstructionFamily::(\w+)\s*:", dispatch))
    d.no_dpp = case_names(required("hasDppForm"), "RdnaOpcode")
    d.no_dpp8 = case_names(required("acceptsDpp8"), "RdnaOpcode")
    return d


def camel(name):
    return "".join(part[:1] + part[1:].lower() for part in name.split("_"))


def decodes(d, name, encoding, form, opcode):
    def table(table_encoding, number):
        return d.tables.get(table_encoding, {}).get(number)

    def base():
        if encoding in ("GLOBAL", "SCRATCH"):
            return table("FLAT", opcode)
        if encoding == "EXP":
            return "Exp" if "EXP" in d.families else None
        hit = table(encoding, opcode)
        if hit:
            return hit
        loose = camel(name)
        return loose if opcode in d.loose.get(loose, ()) else None

    if form == "base":
        return base()
    if form == "vop3":
        direct = table("VOP3", opcode)
        if direct:
            return direct
        if encoding == "VOPC" and opcode <= 0xFF:
            return table("VOPC", opcode)
        if encoding == "VOP2" and 0x100 <= opcode <= 0x13F:
            return None if opcode - 0x100 in d.vop3_vop2_refused else table("VOP2", opcode - 0x100)
        if encoding == "VOP1" and 0x180 <= opcode <= 0x1FF:
            return table("VOP3:VOP1", opcode - 0x180)
        return None
    op = base()
    if not op or op in d.no_dpp or (form == "dpp8" and op in d.no_dpp8):
        return None
    return op


def translator_cases(files):
    text = joined(files, f"{RECOMPILER}Translation/")
    names = set()
    for emit in ("emitScalar", "emitVector", "emitMemory", "emitInterpolation"):
        body = need(body_after(text, re.compile(rf"\bTranslationContext::{emit}\s*\(")), f"TranslationContext::{emit}()")
        names |= case_names(body, "RdnaOpcode")
    if not names:
        raise ScanError("no RdnaOpcode cases in the translator")
    if re.search(r"case\s+RdnaInstructionFamily::EXP\s*:", text):
        names.add("Exp")
    return names


def measure_forms(files, isa):
    decoder = scan_decoder(files)
    translated = translator_cases(files)
    groups, untranslated = {}, set()
    for name, encoding, form, opcode in isa:
        group = groups.setdefault((encoding, form), {"encoding": encoding, "form": form, "total": 0, "decoded": 0, "translated": 0, "missing": [], "untranslated": []})
        group["total"] += 1
        op = decodes(decoder, name, encoding, form, opcode)
        if not op:
            group["missing"].append(name)
            continue
        group["decoded"] += 1
        if op in translated:
            group["translated"] += 1
        else:
            group["untranslated"].append(name)
            untranslated.add(name)
    ordered = sorted(groups.values(), key=lambda g: (g["encoding"], FORMS.index(g["form"])))
    return ordered, sorted(untranslated)


UNSUPPORTED = re.compile(r"unsupported|not (?:yet )?(?:implemented|supported)|unimplemented", re.I)
UNSUPPORTED_TYPES = re.compile(r"^throw\s+(?:\w+::)*(?:UnsupportedInstructionError|\w*Unsupported\w*)\b")
THROW = re.compile(r"""\bthrow\b(?:[^;"']|"(?:\\.|[^"\\\n])*"|'(?:\\.|[^'\\\n])*')*;""")


def unsupported_throws(files):
    sites = []
    for path, raw in files.items():
        if not path.startswith(RECOMPILER) or TEST_PATH.search(path):
            continue
        text = strip_comments(raw)
        for m in THROW.finditer(text):
            statement = m.group(0)
            if statement == "throw;":
                continue
            message = "…".join(re.findall(r'"((?:\\.|[^"\\])*)"', statement))
            if not UNSUPPORTED.search(message) and not UNSUPPORTED_TYPES.search(statement):
                continue
            sites.append({"file": path[len(RECOMPILER):], "line": text.count("\n", 0, m.start()) + 1,
                          "message": message or re.sub(r"\s+", " ", statement)})
    return sorted(sites, key=lambda s: (s["file"], s["line"]))


def image_formats(files):
    enum_text = search(files, "IrBufferFormat enum", lambda text: array_body(text, r"enum\s+class\s+IrBufferFormat\s*:\s*[\w:]+"))
    names = {int(m.group(2)): m.group(1) for m in re.finditer(r"(\w+)\s*=\s*(\d+)", enum_text)}
    defined = search(files, "DefinedImageFormats", lambda text: array_body(text, "DefinedImageFormats"))
    ranges = [(int(m.group(1)), int(m.group(2))) for m in re.finditer(r"\{\s*(\d+)u?\s*,\s*(\d+)u?\s*\}", defined)]
    if not ranges:
        raise ScanError("no DefinedImageFormats ranges")
    info_table = search(files, "kFormatInfoTable", lambda text: array_body(text, "kFormatInfoTable"))
    sampled = set(re.findall(r"\{\s*IrBufferFormat::(\w+)\s*,\s*true\b", info_table))
    buffer = case_names(search(files, "GetFormatInfo()", lambda text: function_body(text, "GetFormatInfo")), "IrBufferFormat")
    if TEXTURE_FORMATS not in files:
        raise ScanError(TEXTURE_FORMATS)
    text = strip_comments(files[TEXTURE_FORMATS])
    host = {int(n) for n in re.findall(r"\{\s*(\d+)\s*,\s*VK_FORMAT_", array_body(text, "kFormatLookup") or "")}
    remap = function_body(text, "remapGuestFormat") or ""
    target = re.search(r"\?\s*(\d+)\s*:", remap)
    if target and int(target.group(1)) in host:
        host.update(int(n) for n in re.findall(r"==\s*(\d+)", remap))
    if not host:
        raise ScanError("no kFormatLookup entries")
    values = [value for first, last in ranges for value in range(first, last + 1)]
    return {"total": len(values),
            "sampled": sum(1 for v in values if names.get(v) in sampled),
            "buffer": sum(1 for v in values if names.get(v) in buffer),
            "host": sum(1 for v in values if v in host)}


def throw_areas(sites):
    counts = {}
    for site in sites:
        name = site["file"].split("/", 1)[0] if "/" in site["file"] else "Recompiler"
        counts[name] = counts.get(name, 0) + 1
    return [{"name": name, "count": count} for name, count in sorted(counts.items(), key=lambda item: -item[1])]


def read_tree(root):
    files = {}
    for directory in SCANNED_DIRECTORIES:
        base = Path(root) / directory
        for path in base.rglob("*"):
            if path.is_file():
                files[path.relative_to(root).as_posix()] = path.read_text(encoding="utf-8", errors="replace")
    return dict(sorted(files.items()))


def measure(root, isa):
    files = read_tree(root)
    forms, untranslated = measure_forms(files, isa)
    plain = [f for f in forms if f["form"] in ("base", "vop3")]
    dpp = [f for f in forms if f["form"] in ("dpp16", "dpp8")]
    total = lambda rows, key: sum(r[key] for r in rows)
    throws = unsupported_throws(files)
    return {
        "isa": "AMD RDNA 2",
        "instructions": {"total": total(plain, "total"), "decoded": total(plain, "decoded"), "translated": total(plain, "translated")},
        "dpp": {"total": total(dpp, "total"), "decoded": total(dpp, "decoded")},
        "unsupported_throws": len(throws),
        "throw_areas": throw_areas(throws),
        "throw_sites": throws,
        "formats": image_formats(files),
        "forms": forms,
        "untranslated": untranslated,
    }
