import argparse
import importlib.util
import json
import os
import re
import sys
from fractions import Fraction
from html import escape
from pathlib import Path

tokenizer_spec = importlib.util.spec_from_file_location("anyps5_cpp_tokenizer", Path(__file__).with_name("nid_names.py"))
tokenizer = importlib.util.module_from_spec(tokenizer_spec)
tokenizer_spec.loader.exec_module(tokenizer)
CPP_TOKEN = tokenizer.CPP_TOKEN
origin_spec = importlib.util.spec_from_file_location("anyps5_stub_origins", Path(__file__).with_name("stub_origins.py"))
stub_origins = importlib.util.module_from_spec(origin_spec)
origin_spec.loader.exec_module(stub_origins)

ROOT = Path(__file__).resolve().parent.parent
PRX = ROOT / "core" / "libs" / "prx"
OPCODES = ROOT / "core" / "shader" / "recompiler" / "RdnaDecoder" / "include" / "RdnaDecoder" / "RdnaOpcode.hpp"
ISA = Path(__file__).resolve().parent / "rdna_isa.txt"
NID = re.compile(r"[A-Za-z0-9+\-]{11}")
TAGS = ("audit", "loader", "manual")
AUDIT_DETAIL = re.compile(r"(?:import_audit|audit_dump) \d{4}-\d{2}-\d{2}")
TAG_COMMENT = re.compile(r"([A-Za-z][\w-]*)(?::\s*(.*))?")
UNTAGGED = "untagged"
OBSERVED_RELATIVE = Path("docs/dev/observed")
OBSERVED = ROOT / OBSERVED_RELATIVE
INVENTORY_RELATIVE = Path("docs/dev/inventory")
INVENTORY = ROOT / INVENTORY_RELATIVE
SOURCE_LABEL = re.compile(r"[A-Za-z0-9][A-Za-z0-9._+-]*")
SOURCE = f'https://github.com/{os.environ.get("GITHUB_REPOSITORY", "boykopovar/AnyPS5")}/blob/main'
DEFINITION = re.compile(r"\bAPS5_VABI\s+(\w+)\s*\([^;{]*\)\s*(?:noexcept\s*)?(?:try\s*)?\{")
EXPORT = re.compile(r"\bAPS5_EXPORT\s*\(\s*LITERAL\s*,\s*(\w+)\s*\)")
STUB = "NotImplemented_nid_no_patch"
CALL = re.compile(r"\b(\w+)\s*\(")
KEYWORDS = r"return|else|do|throw|co_return|co_await|co_yield|and|or|not|xor|bitand|bitor|compl"
QUALIFIED = re.compile(rf"(?:\.|->|(?:\b(?!(?:{KEYWORDS})\b)\w+|>)\s*::)\s*$")
CMAKE_SHARED = re.compile(r'^\s*include\(\s*"?\$\{CMAKE_CURRENT_SOURCE_DIR\}/\.\./([\w.-]+)/[\w.-]+\.cmake"?\s*\)', re.M)
SOURCE_SHARED = re.compile(r'^\s*#\s*include\s*"prx/([\w.-]+)/[\w./-]+\.cpp"', re.M)
STUB_WRAPPER = re.compile(
    r"^[ \t]*(?:static\s+)?(?:\[\[noreturn\]\]\s+)?(?:(?!APS5_VABI)[\w:<>,\s*&])+?[\s*&]"
    r"(?!(?:if|while|for|switch|catch|return|else)\b)(\w+)\s*\([^;{]*\)\s*"
    r"(?:const\s*)?(?:noexcept\s*)?(?:->\s*[\w:<>*&\s]+?)?\{", re.M)
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
PANEL_WIDTH, GAP, MAP_HEIGHT, HEADER, LEGEND = 495, 10, 280, 30, 26
DONE_COLOR, TODO_COLOR, UNOBSERVED_COLOR, BORDER, TEXT = "#2ea043", "#da3633", "#6e7681", "#0d1117", "#ffffff"


def blank(match):
    if match.group("code") is not None:
        return match.group()
    if match.group("comment") is None:
        return "LITERAL"
    return re.sub(r"[^\n]", " ", match.group())


def code(text):
    return CPP_TOKEN.sub(blank, text.replace("\\\n", "").replace("\\\r\n", ""))

def body_end(text, start):
    depth = 0
    for i in range(start, len(text)):
        if text[i] == "{":
            depth += 1
        elif text[i] == "}":
            depth -= 1
            if depth == 0:
                return i
    return len(text)


def stub_calls(text):
    helpers = {}
    for match in STUB_WRAPPER.finditer(text):
        body = text[match.end() - 1:body_end(text, match.end() - 1)]
        helpers.setdefault(match.group(1), set()).update(call.group(1) for call in CALL.finditer(body))
    calls = {STUB}
    while True:
        found = {name for name, targets in helpers.items() if name not in calls and targets & calls}
        if not found:
            return calls
        calls |= found


def statements(body):
    depth, start, parts = 0, 1, []
    for i, character in enumerate(body[1:], 1):
        if character == "{":
            depth += 1
        elif character == "}":
            depth -= 1
        elif character == ";" and not depth:
            parts.append(body[start:i + 1])
            start = i + 1
    parts.append(body[start:])
    return parts


def inert_statement(statement, wrappers):
    if EMPTY_RETURN.fullmatch(statement) or CONST_RETURN.fullmatch(statement) or BARE_THROW.fullmatch(statement):
        return True
    call = WRAPPER_RETURN.fullmatch(statement)
    return bool(call) and call.group(1) in wrappers


def inert_body(body, wrappers):
    terminals = 0
    for statement in statements(body):
        statement = statement.strip()
        if not statement:
            continue
        if DISCARD.fullmatch(statement):
            continue
        if terminals or not inert_statement(statement, wrappers):
            return False
        terminals += 1
    return True


def inert_wrappers(text):
    bodies = {m.group(1): text[m.end() - 1:body_end(text, m.end() - 1)] for m in STUB_WRAPPER.finditer(text)}
    names = set()
    while True:
        found = {name for name, body in bodies.items() if name not in names and inert_body(body, names)}
        if not found:
            return names
        names |= found


def manifest_entries(text):
    for number, line in enumerate(text.splitlines(), 1):
        entry = line.split("#", 1)[0].strip()
        if entry:
            yield number, entry


def manifest_problems(text):
    problems, seen, previous = [], set(), ""
    for number, entry in manifest_entries(text):
        if NID.fullmatch(entry) is None:
            problems.append((number, f"invalid NID {entry!r}"))
            continue
        if entry in seen:
            problems.append((number, f"duplicate NID {entry}"))
        elif entry < previous:
            problems.append((number, f"{entry} is not in sorted order after {previous}"))
        seen.add(entry)
        previous = max(previous, entry)
    return problems


def classify_comment(comment):
    comment = comment.strip()
    match = TAG_COMMENT.fullmatch(comment)
    if not match or (match.group(2) is None and " " in comment):
        return UNTAGGED, "", None
    tag, detail = match.group(1), (match.group(2) or "").strip()
    problem = None
    if tag not in TAGS:
        problem = f"unknown provenance tag {tag!r}; use one of {', '.join(TAGS)}"
    elif tag == "audit" and not AUDIT_DETAIL.fullmatch(detail):
        problem = "audit needs the tool and the date of the run: audit: import_audit YYYY-MM-DD or audit: audit_dump YYYY-MM-DD"
    elif tag == "manual" and not detail:
        problem = "manual needs a written reason: manual: <reason>"
    return tag, detail, problem


def manifest_tags(text):
    for number, line in enumerate(text.splitlines(), 1):
        entry, _, comment = line.partition("#")
        entry = entry.strip()
        if NID.fullmatch(entry) is not None:
            yield (number, entry, *classify_comment(comment))


def manifest_tag_problems(text):
    return [(number, problem) for number, _, _, _, problem in manifest_tags(text) if problem]


def manifest_untagged(text):
    return [(number, entry) for number, entry, tag, _, _ in manifest_tags(text) if tag == UNTAGGED]


def read_provenance(path):
    provenance = {}
    for _, entry, tag, _, _ in manifest_tags(path.read_text(encoding="utf-8-sig")):
        provenance.setdefault(entry, tag)
    return provenance


def inventory_entries(text):
    for number, line in enumerate(text.splitlines(), 1):
        entry = line.split("#", 1)[0].split()
        if entry:
            yield number, entry


def inventory_problems(text):
    problems, seen, previous = [], set(), ""
    for number, entry in inventory_entries(text):
        if len(entry) != 2 or NID.fullmatch(entry[0]) is None or SOURCE_LABEL.fullmatch(entry[1]) is None:
            problems.append((number, "expected '<NID> <source>' with an 11-character NID and a source name of letters, digits and . _ + -"))
            continue
        if entry[0] in seen:
            problems.append((number, f"duplicate NID {entry[0]}"))
        elif entry[0] < previous:
            problems.append((number, f"{entry[0]} is not in sorted order after {previous}"))
        seen.add(entry[0])
        previous = max(previous, entry[0])
    return problems


def read_inventory(path):
    text = path.read_text(encoding="utf-8-sig")
    problems = inventory_problems(text)
    if problems:
        raise ValueError(f"{path}:{problems[0][0]}: {problems[0][1]}")
    return {entry[0]: entry[1] for _, entry in inventory_entries(text)}


def read_manifest(path):
    observed = set()
    for number, entry in manifest_entries(path.read_text(encoding="utf-8-sig")):
        if NID.fullmatch(entry) is None:
            raise ValueError(f"{path}:{number}: invalid NID {entry!r}")
        observed.add(entry)
    return frozenset(observed)


def scan_library(path, observed=frozenset(), provenance=None, inventory=None):
    done, todo, unobserved, inert, live = set(), set(), set(), set(), set()
    raw = [source.read_text(encoding="utf-8", errors="ignore") for source in path.rglob("*.cpp")
           if "tests" not in source.relative_to(path).parts]
    texts = [code(text) for text in raw]
    exported = {match.group(1) for text in texts for match in EXPORT.finditer(text)}
    aliases = {}
    for text in raw:
        for nid, name in tokenizer.source_aliases(text):
            aliases.setdefault(name, set()).add(nid)
    for text in texts:
        calls = stub_calls(text)
        wrappers = inert_wrappers(text)
        for match in DEFINITION.finditer(text):
            name = match.group(1)
            if name.endswith(("_nid_no_patch", "_nid_no_patch_cut")) and name not in exported:
                continue
            body = text[match.end() - 1:body_end(text, match.end() - 1)]
            if any(call.group(1) in calls and not QUALIFIED.search(body, 0, call.start())
                   for call in CALL.finditer(body)):
                if tokenizer.nids_of(name, aliases) & observed:
                    todo.add(name)
                else:
                    unobserved.add(name)
                continue
            done.add(name)
            (inert if inert_body(body, wrappers) else live).add(name)
    todo -= done
    unobserved -= done
    inert -= live
    group = {"name": path.name, "label": path.name.removeprefix("libSce"),
             "done": len(done), "todo": len(todo), "unobserved": len(unobserved),
             "done_names": sorted(done), "todo_names": sorted(todo), "unobserved_names": sorted(unobserved),
             "inert_names": sorted(inert)}
    group["done_unobserved_names"] = sorted(name for name in done if not tokenizer.nids_of(name, aliases) & observed)
    if inventory:
        sources = {name: inventory[nid] for name in sorted(todo | unobserved) for nid in sorted(tokenizer.nids_of(name, aliases)) if nid in inventory}
        if sources:
            group["inventory"] = sources
    if provenance is not None and todo:
        group["observed_tags"] = {name: provenance.get(min(tokenizer.nids_of(name, aliases) & observed), UNTAGGED) for name in sorted(todo)}
    if not (done or todo or unobserved):
        cmake = path / "CMakeLists.txt"
        texts = [(CMAKE_SHARED, cmake.read_text(encoding="utf-8", errors="ignore"))] if cmake.is_file() else []
        texts += [(SOURCE_SHARED, source.read_text(encoding="utf-8", errors="ignore")) for source in sorted(path.glob("*.cpp"))]
        owners = [m[1] for pattern, content in texts for m in pattern.finditer(content)]
        owners = [o for o in owners if o != path.name and (path.parent / o).is_dir()]
        if owners:
            group["shared_sources"] = owners[0]
    return group


def summarize(groups):
    done = sum(g["done"] for g in groups)
    total = done + sum(g["todo"] for g in groups)
    unobserved = sum(g.get("unobserved", 0) for g in groups)
    declared = total + unobserved
    result = {"done": done, "total": total, "percent": round(100 * done / total, 2) if total else 0,
              "unobserved": unobserved, "declared_total": declared,
              "declared_percent": round(100 * done / declared, 2) if declared else 0, "groups": groups}
    tags = {}
    for group in groups:
        for tag in group.get("observed_tags", {}).values():
            tags[tag] = tags.get(tag, 0) + 1
    if tags:
        result["observed_by_tag"] = dict(sorted(tags.items()))
    return result


def observed_nids(library):
    manifest = OBSERVED / f"{library.name}.txt"
    return read_manifest(manifest) if manifest.is_file() else frozenset()


def inventory_sources(library):
    listing = INVENTORY / f"{library.name}.txt"
    return read_inventory(listing) if listing.is_file() else None


def observed_provenance(library):
    manifest = OBSERVED / f"{library.name}.txt"
    return read_provenance(manifest) if manifest.is_file() else None


def collect_libraries():
    return summarize([scan_library(p, observed_nids(p), observed_provenance(p), inventory_sources(p)) for p in sorted(PRX.iterdir()) if p.is_dir()])


def add_origins(libraries, root):
    numbers = stub_origins.pull_requests(root)
    for group in libraries["groups"]:
        names = set(group.get("done_names", ())) | set(group.get("todo_names", ())) | set(group.get("unobserved_names", ()))
        if names:
            found = stub_origins.origins(root, PRX / group["name"], names, numbers)
            if found:
                group["origins"] = dict(sorted(found.items()))


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
            name, encoding = line.split()
            isa[name] = encoding
    by_camel = {camel(name): name for name in isa}
    enum = re.search(r"enum class RdnaOpcode[^{]*\{(.*?)\};", OPCODES.read_text(encoding="utf-8", errors="ignore"), re.S).group(1)
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


def rect(x, y, w, h, color, stroke=0.5, title=None):
    shape = f'<rect x="{x:.2f}" y="{y:.2f}" width="{w:.2f}" height="{h:.2f}" fill="{color}" stroke="{BORDER}" stroke-width="{stroke}"'
    return f"{shape}><title>{escape(title)}</title></rect>" if title else f"{shape}/>"


def commit_url(sha):
    return SOURCE.rsplit("/blob/", 1)[0] + "/commit/" + sha


def pull_request_url(number):
    return SOURCE.rsplit("/blob/", 1)[0] + f"/pull/{number}"


def origin_text(origin):
    text = f"{origin['commit'][:10]} on {origin['date']}"
    if origin.get("time"):
        text += f" {origin['time']}"
    if origin.get("author"):
        text += f" by {origin['author']}"
    if origin.get("pr"):
        text += f" in PR #{origin['pr']}"
    return text


def cell_title(group, name, kind):
    details = [f"{name}: {kind}"]
    tag = group.get("observed_tags", {}).get(name)
    if tag:
        details.append(f"observed by {tag}")
    origin = group.get("origins", {}).get(name)
    if origin:
        details.append(f"declared in {origin_text(origin)}")
    source = group.get("inventory", {}).get(name)
    if source:
        details.append(f"listed in {source}")
    return ", ".join(details) if len(details) > 1 else None


def text(x, y, value, size):
    return (f'<text x="{x:.2f}" y="{y:.2f}" font-family="sans-serif" font-size="{size}" fill="{TEXT}" '
            f'stroke="{BORDER}" stroke-width="3" paint-order="stroke">{escape(value)}</text>')


def declared_size(group):
    return group["done"] + group["todo"] + group.get("unobserved", 0)


def treemap(title, data, left, states=True, detail=False):
    groups = sorted((g for g in data["groups"] if declared_size(g)), key=lambda g: -declared_size(g))
    parts = [text(left + 4, 21, f'{title}: {data["percent"]}% ({data["done"]}/{data["total"]})', 16)]
    for group, (x, y, w, h) in zip(groups, squarify([declared_size(g) for g in groups], left, HEADER, PANEL_WIDTH, MAP_HEIGHT)):
        observed = group["done"] + group["todo"]
        unobserved = group.get("unobserved", 0)
        shared = [g["name"] for g in data["groups"] if g.get("shared_sources") == group["name"]]
        suffix = "; shared by " + ", ".join(shared) if shared else ""
        ratio = f" ({100 * group['done'] / observed:.0f}%)" if observed else ""
        extra = f", {unobserved} unobserved" if unobserved else ""
        parts.append(f'<g><title>{escape(group["name"])}: {group["done"]}/{observed}{ratio}{escape(extra)}{escape(suffix)}</title>')
        pending = group.get("todo_names", []) if states else []
        hidden = group.get("unobserved_names", []) if states else []
        for i, cell in enumerate(cells(declared_size(group), x + 1, y + 1, w - 2, h - 2)):
            if i < group["done"]:
                parts.append(rect(*cell, DONE_COLOR))
            elif i < observed:
                index = i - group["done"]
                name = pending[index] if index < len(pending) else None
                parts.append(rect(*cell, TODO_COLOR if states else UNOBSERVED_COLOR, title=cell_title(group, name, "observed stub") if name and detail else None))
            else:
                index = i - observed
                name = hidden[index] if index < len(hidden) else None
                parts.append(rect(*cell, UNOBSERVED_COLOR, title=cell_title(group, name, "unobserved stub") if name and detail else None))
        parts.append(f'<rect x="{x:.2f}" y="{y:.2f}" width="{w:.2f}" height="{h:.2f}" fill="none" stroke="{BORDER}" stroke-width="2"/>')
        label_length = int((w - 10) / 7)
        label = group["label"][:label_length] if label_length > 0 else ""
        if (label == group["label"] or len(label) >= 3) and h > 22:
            parts.append(text(x + 5, y + 16, label, 12))
        parts.append("</g>")
    return parts


def legend(left, top):
    parts, x = [], left
    for label, color in (("Implemented", DONE_COLOR), ("Observed stub", TODO_COLOR),
                         ("Unobserved stub (libraries) / Not implemented (shaders)", UNOBSERVED_COLOR)):
        parts.append(rect(x, top + 7, 12, 12, color, 0.5))
        parts.append(text(x + 17, top + 18, label, 12))
        x += 17 + 7 * len(label) + 18
    return parts


def traced(libraries):
    return any(group.get(field) for group in libraries["groups"] for field in ("origins", "observed_tags", "inventory"))


def render(libraries, shaders, detail=False):
    width, height = 2 * PANEL_WIDTH + GAP, HEADER + MAP_HEIGHT + LEGEND
    parts = [f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {width} {height}" width="{width}" height="{height}">',
             rect(0, 0, width, height, BORDER, 0)]
    parts += treemap("System libraries*", libraries, 0, detail=detail)
    parts += treemap("GPU shader instructions", shaders, PANEL_WIDTH + GAP, states=False)
    parts += legend(4, HEADER + MAP_HEIGHT)
    parts.append("</svg>")
    return "\n".join(parts)


def badge(label, data):
    percent = data["percent"]
    color = "#4c1" if percent >= 90 else "#97ca00" if percent >= 60 else "#dfb317" if percent >= 30 else "#fe7d37"
    value = f"{percent}%"
    left, right = 10 + 7 * len(label), 10 + 7 * len(value)
    width = left + right
    return (f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="20" role="img" aria-label="{label}: {value}">'
            f'<rect width="{left}" height="20" fill="#555"/><rect x="{left}" width="{right}" height="20" fill="{color}"/>'
            f'<g fill="#fff" font-family="Verdana,DejaVu Sans,sans-serif" font-size="11" text-anchor="middle">'
            f'<text x="{left / 2}" y="14">{label}</text><text x="{left + right / 2}" y="14">{value}</text></g></svg>')


def table(heading, column, data, states=False):
    if states:
        header = f"<tr><th>{column}</th><th>Implemented</th><th>Observed stubs</th><th>Unobserved stubs</th><th>%</th><th>Declared %</th></tr>"
        span = 5
    else:
        header = f"<tr><th>{column}</th><th>Implemented</th><th>Total</th><th>%</th></tr>"
        span = 3
    rows = [f"<h2>{heading}</h2>", "<table>", header]
    for group in sorted(data["groups"], key=lambda g: g["name"].lower()):
        if "shared_sources" in group:
            owner = escape(group["shared_sources"])
            rows.append(f'<tr><td>{escape(group["name"])}</td><td colspan="{span}">Shared sources: '
                        f'<a href="{SOURCE}/core/libs/prx/{owner}">{owner}</a></td></tr>')
            continue
        observed = group["done"] + group["todo"]
        declared = observed + group.get("unobserved", 0)
        if not declared:
            rows.append(f'<tr><td>{escape(group["name"])}</td><td colspan="{span}">No exports</td></tr>')
            continue
        percent = f'{100 * group["done"] / observed:.0f}%' if observed else "n/a"
        if not states:
            rows.append(f'<tr><td>{escape(group["name"])}</td><td>{group["done"]}</td><td>{observed}</td><td>{percent}</td></tr>')
            continue
        rows.append(f'<tr><td>{escape(group["name"])}</td><td>{group["done"]}</td><td>{group["todo"]}</td>'
                    f'<td>{group.get("unobserved", 0)}</td><td>{percent}</td><td>{100 * group["done"] / declared:.0f}%</td></tr>')
    if states:
        rows.append(f'<tr><th>Total</th><th>{data["done"]}</th><th>{data["total"] - data["done"]}</th><th>{data["unobserved"]}</th>'
                    f'<th>{data["percent"]}%</th><th>{data["declared_percent"]}%</th></tr>')
    else:
        rows.append(f'<tr><th>Total</th><th>{data["done"]}</th><th>{data["total"]}</th><th>{data["percent"]}%</th></tr>')
    rows.append("</table>")
    return "\n".join(rows)


def swatch(color):
    return f'<span style="display:inline-block;width:12px;height:12px;background:{color};margin-right:6px"></span>'


def legend_block():
    return "\n".join([
        '<div class="legend"><h2>How to read the graphs</h2><ul style="list-style:none;padding-left:0">',
        f'<li>{swatch(DONE_COLOR)}<b>Green: implemented.</b> The function no longer calls <code>NotImplemented_nid_no_patch</code>.</li>',
        f'<li>{swatch(TODO_COLOR)}<b>Red: observed stub.</b> Declared but not implemented, and a measured use shows it is needed.</li>',
        f'<li>{swatch(UNOBSERVED_COLOR)}<b>Grey: unobserved stub</b> (in the shader panel, an instruction that is not implemented). Declared but not '
        'implemented, and no measured use is known; this does not mean no title uses it.</li></ul>',
        '<p><b>Headline</b> = implemented / (implemented + observed stubs), so grey stubs do not lower it. <b>Declared</b> = implemented / '
        '(implemented + observed + unobserved), every declared function counted. Unobserved means no measured use is known.</p></div>'])


def stub_sentence(libraries):
    pending = libraries["total"] - libraries["done"] + libraries.get("unobserved", 0)
    sentence = f'{libraries.get("unobserved", 0)} of the {pending} declared functions that are not implemented are unobserved stubs.'
    tags = libraries.get("observed_by_tag")
    if tags:
        sentence += " Observed stubs by provenance: " + ", ".join(f"{tag} {count}" for tag, count in tags.items()) + "."
    return sentence


def summary(libraries, shaders):
    extra = ", ".join(f"<code>{escape(o)}</code>" for o in shaders["extra"])
    return "\n".join([
        '<!DOCTYPE html>',
        '<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1">',
        '<title>AnyPS5 progress</title>',
        '<style>body{font-family:sans-serif;max-width:1000px;margin:auto;padding:16px}img{max-width:100%}'
        'table{border-collapse:collapse}th,td{border:1px solid #ccc;padding:2px 8px}td+td,th+th{text-align:right}</style>',
        '</head><body>',
        "<h1>Progress</h1>",
        '<img src="progress.svg" alt="progress">',
        legend_block(),
        *(['<p>Open the <a href="progress-detail.svg">detailed map</a> and hover a pending cell for the function\'s state, '
           'provenance, origin commit and date, and inventory source.</p>'] if traced(libraries) else []),
        f'<p>Generated by <a href="{SOURCE}/tools/progress.py">tools/progress.py</a> on every push to <code>main</code>. '
        'Raw numbers: <a href="progress.json">progress.json</a>.</p>',
        table("System libraries", "Library", libraries, states=True),
        f'<p>{stub_sentence(libraries)}</p>',
        "<p>A function is implemented when it no longer calls <code>NotImplemented_nid_no_patch</code>. "
        "A pending function is an observed stub when any measured use shows its NID is needed, and an unobserved stub otherwise. "
        "The percentage counts implemented functions over implemented and observed stubs, so unobserved stubs do not lower it; "
        "the declared percentage counts them too. A library with no list of observed NIDs has only unobserved stubs; "
        f'<a href="{SOURCE}/docs/dev/PROGRESS.md#observed-imports">how the lists work</a>. '
        "Comparison reports separately mark implemented functions whose body only returns a constant or throws. "
        f'The total only includes functions already declared in <a href="{SOURCE}/core/libs/prx">core/libs/prx</a>, '
        "not every function exported by the PS5 firmware. Shared-source libraries are counted only at their source library. "
        "Libraries with no exports only provide an empty module for titles that load them. "
        f'<a href="{SOURCE}/docs/dev/PROGRESS.md">Counting rules</a>.</p>',
        table("GPU shader instructions", "Encoding", shaders),
        f'<p>The total is the AMD RDNA 1 + RDNA 2 instruction list (<a href="{SOURCE}/tools/rdna_isa.txt">tools/rdna_isa.txt</a>). '
        f'An instruction is implemented when the <a href="{SOURCE}/core/shader/recompiler/RdnaDecoder">decoder</a> recognizes it. '
        f"Decoded opcodes not present in AMD's public list are not counted: {extra}.</p>",
        '<p><a href="history.html">Progress history</a> (one point per day).</p>',
        "</body></html>",
    ]) + "\n"


def names(data, state, owners=None):
    owners = owners or {}
    return {(owners.get(g["name"], g["name"]), n) for g in data["groups"] for n in g.get(f"{state}_names", [])}


def origin_map(data, owners=None):
    owners = owners or {}
    return {(owners.get(g["name"], g["name"]), n): origin for g in data["groups"] for n, origin in g.get("origins", {}).items()}


def origin_link(origin):
    text = f"[`{origin['commit'][:10]}`]({commit_url(origin['commit'])}) {origin['date']}"
    if origin.get("time"):
        text += f" {origin['time']}"
    if origin.get("author"):
        text += f" by {origin['author']}"
    if origin.get("pr"):
        text += f" in [#{origin['pr']}]({pull_request_url(origin['pr'])})"
    return text


def details(icon, title, column, items, origins=None):
    if not items:
        return []
    shown = sorted(items)[:REPORT_ROWS]
    linked = bool(origins) and any(item in origins for item in shown)
    if linked:
        rows = [f"<details>\n<summary>{icon} {len(items)} {title}</summary>\n", f"| {column} | Name | Origin |", "| - | - | - |"]
        rows += [f"| {group} | `{name}` | {origin_link(origins[(group, name)]) if (group, name) in origins else ''} |" for group, name in shown]
    else:
        rows = [f"<details>\n<summary>{icon} {len(items)} {title}</summary>\n", f"| {column} | Name |", "| - | - |"]
        rows += [f"| {group} | `{name}` |" for group, name in shown]
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
    base_observed, head_observed = names(base, "todo", owners), names(head, "todo", owners)
    base_unobserved, head_unobserved = names(base, "unobserved", owners), names(head, "unobserved", owners)
    base_all, head_all = base_done | base_observed | base_unobserved, head_done | head_observed | head_unobserved
    implemented = head_done - base_done
    declared = head_all - base_all - head_done
    declared_observed, declared_unobserved = declared & head_observed, declared & head_unobserved
    regressed, removed = base_done & (head_all - head_done), base_all - head_all
    inert = implemented & names(head, "inert", owners)
    unseen = implemented & names(head, "done_unobserved", owners)
    origins = origin_map(head, owners)
    observed_total = lambda data: data.get("total", 0) - data.get("done", 0)
    split_changed = column == "Library" and (observed_total(base) != observed_total(head) or base.get("unobserved", 0) != head.get("unobserved", 0))
    if not (implemented or declared or regressed or removed or shared or split_changed or base.get("percent") != head.get("percent")):
        return []
    delta = round(head["percent"] - base["percent"], 2)
    exact = exact_delta(base, head) if delta == 0 else None
    shown = small_delta(exact) if exact else f"{delta:+}"
    icon = "📈" if (exact or delta) > 0 else "📉" if (exact or delta) < 0 else "➖"
    stubs = ("observed stubs", "unobserved stubs") if column == "Library" else ("declared", "declared")
    counts = [f"{n:+} {label}{suffix}" for n, label, suffix in ((len(implemented), "implemented", f" {unit}"), (len(declared_observed), stubs[0], ""),
                                                              (len(declared_unobserved), stubs[1], ""), (-len(regressed), "reverted", f" {unit}"),
                                                              (-len(removed), "removed", f" {unit}")) if n]
    if split_changed:
        counts.append(f'{observed_total(head)} observed stubs in total, {head.get("unobserved", 0)} unobserved stubs in total')
        counts.append(f'declared {base.get("declared_percent", base["percent"])}% \u2192 {head.get("declared_percent", head["percent"])}%')
    changes = f', {", ".join(counts)}' if counts else ""
    lines = [f'{icon} **{title}**: {head["percent"]}% ({shown}%{changes})', ""]
    if unseen and column == "Library":
        noun = "function" if len(unseen) == 1 else "functions"
        lines += [f"⚠️ {len(unseen)} newly implemented {noun} with no measured use: add each one to docs/dev/observed with its reason in this PR, or name the observed function that needs it.", ""]
    lines += details("✅", "implemented", column, implemented)
    lines += details("⚪", "behaviourally empty (constant return or throw)", column, inert)
    lines += details("🔍", "implemented but not observed anywhere", column, unseen)
    lines += details("🆕", "declared as observed stubs", column, declared_observed, origins)
    lines += details("👻", "declared as unobserved stubs (no measured use needs them)", column, declared_unobserved, origins)
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
    unobserved = group.get("unobserved", 0)
    total = group["done"] + group["todo"]
    if not total and not unobserved:
        return "No exports", None
    suffix = f", +{unobserved} unobserved" if unobserved else ""
    if not total:
        return f"0/0 observed{suffix}", None
    percent = 100 * group["done"] / total
    return f'{group["done"]}/{total} ({percent:.2f}%){suffix}', percent


def percent_change(old_percent, new_percent):
    if old_percent is None or new_percent is None:
        return "n/a"
    return f'{round(new_percent - old_percent, 2) or 0:+.2f}%'


def library_report(base, head):
    before, after = ({group["name"]: group for group in data["groups"]} for data in (base, head))
    fields = ("done", "todo", "unobserved", "done_names", "todo_names", "unobserved_names", "shared_sources")
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
             "Before and After are implemented over implemented plus observed stubs. A library with no observed stub shows 100%: "
             "unobserved is the default for a stub with no measured use, so nothing is counted as pending there.", "",
             "| Library | Before | After | Change |", "| - | - | - | - |"] + rows[:REPORT_ROWS]
    if len(rows) > REPORT_ROWS:
        lines.append(f"| ... | {len(rows) - REPORT_ROWS} more libraries | | |")
    return lines + [""]


def report(base, head):
    lines = compare("System libraries", "Library", "functions", base["libraries"], head["libraries"])
    lines += library_report(base["libraries"], head["libraries"])
    lines += compare("GPU shader instructions", "Encoding", "instructions", base["shaders"], head["shaders"])
    return "\n".join(lines)


SITE_REPORTS = ("progress.json", "badge-libraries.svg", "badge-shaders.svg", "progress.svg", "progress-detail.svg", "progress.html")


def require_distinct_reports(output):
    paths = [output / name for name in SITE_REPORTS]
    for index, path in enumerate(paths):
        if not path.exists():
            continue
        for other in paths[index + 1:]:
            if other.exists() and path.samefile(other):
                raise ValueError(f"{path.name} and {other.name} in {output} are the same file")


def write_site(output, libraries, shaders):
    output.mkdir(parents=True, exist_ok=True)
    require_distinct_reports(output)
    (output / "progress.json").write_text(json.dumps({"libraries": libraries, "shaders": shaders}, indent=2), encoding="utf-8")
    (output / "badge-libraries.svg").write_text(badge("libraries (headline)*", libraries), encoding="utf-8")
    (output / "badge-shaders.svg").write_text(badge("shaders", shaders), encoding="utf-8")
    (output / "progress.svg").write_text(render(libraries, shaders), encoding="utf-8")
    if traced(libraries):
        (output / "progress-detail.svg").write_text(render(libraries, shaders, detail=True), encoding="utf-8")
    (output / "progress.html").write_text(summary(libraries, shaders), encoding="utf-8")


def validate_snapshot(snapshot):
    if not isinstance(snapshot, dict):
        raise ValueError("snapshot must be an object")
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
        declared = data.get("declared_percent", 0)
        if isinstance(declared, bool) or not isinstance(declared, (int, float)) or not 0 <= declared <= 100:
            raise ValueError(f"{section}.declared_percent must be a finite number between 0 and 100")
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
                for field in ("done", "todo", "unobserved"):
                    if field in group and (type(group[field]) is not int or group[field] < 0):
                        raise ValueError(f"{location}.{field} must be a non-negative integer")
                if "classified" in group and type(group["classified"]) is not bool:
                    raise ValueError(f"{location}.classified must be a boolean")
            for field in ("done_names", "todo_names", "inert_names", "unobserved_names", "done_unobserved_names"):
                values = group.get(field, [])
                if not isinstance(values, list) or any(not isinstance(value, str) for value in values):
                    raise ValueError(f"{location}.{field} must be a list of strings")
            for field in ("observed_tags", "origins", "inventory"):
                values = group.get(field, {})
                if not isinstance(values, dict) or any(not isinstance(key, str) for key in values):
                    raise ValueError(f"{location}.{field} must be an object keyed by function name")
            if any(not isinstance(tag, str) for tag in group.get("observed_tags", {}).values()):
                raise ValueError(f"{location}.observed_tags must map names to strings")


def main(argv=None):
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8", errors="backslashreplace")
    if hasattr(sys.stderr, "reconfigure"):
        sys.stderr.reconfigure(encoding="utf-8", errors="backslashreplace")
    global PRX, OPCODES, OBSERVED, INVENTORY
    previous = PRX, OPCODES, OBSERVED, INVENTORY
    parser = argparse.ArgumentParser()
    parser.add_argument("output", type=Path, nargs="?")
    parser.add_argument("--root", type=Path, help="source tree to measure instead of the one containing this script")
    parser.add_argument("--compare", type=Path, nargs=2, metavar=("BASE", "HEAD"),
                        help="print a markdown report of the changes between two progress.json files")
    parser.add_argument("--module", action="append", metavar="NAME",
                        help="limit library counts to these core/libs/prx directories; repeat to include several. Shader counts stay complete")
    parser.add_argument("--origins", action="store_true",
                        help="add an origin (commit and date of the declaration, from git blame) to every function of each library; needs a git work tree")
    parser.add_argument("--json", action="store_true",
                        help="print the progress.json object to stdout instead of the summary lines")
    try:
        args = parser.parse_args(argv)
        if args.compare:
            if args.module or args.json:
                parser.error("--compare does not combine with --module or --json")
            snapshots = []
            for path in args.compare:
                try:
                    snapshots.append(json.loads(path.read_text(encoding="utf-8-sig")))
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
        if not args.output and not (args.module or args.json):
            parser.error("the output directory is required")
        if args.root:
            PRX = args.root / "core" / "libs" / "prx"
            OPCODES = args.root / previous[1].relative_to(ROOT)
            OBSERVED = args.root / OBSERVED_RELATIVE
            INVENTORY = args.root / INVENTORY_RELATIVE
        try:
            libraries, shaders = collect_libraries(), collect_shaders()
        except FileNotFoundError as error:
            parser.error(f"{args.root or ROOT} is not a source tree: {error.filename} not found")
        except (OSError, ValueError) as error:
            parser.error(str(error))
        try:
            libraries = select_libraries(libraries, args.module or [])
        except ValueError as error:
            parser.error(str(error))
        if args.origins:
            if not stub_origins.available(args.root or ROOT):
                parser.error(f"--origins needs a git work tree: {args.root or ROOT} is not one")
            add_origins(libraries, args.root or ROOT)
        if args.output:
            try:
                write_site(args.output, libraries, shaders)
            except ValueError as error:
                parser.error(str(error))
            except OSError as error:
                parser.error(f"{args.output}: {error.strerror or error}")
        if args.json:
            print(json.dumps({"libraries": libraries, "shaders": shaders}, indent=2))
        else:
            declared = f', declared {libraries["declared_percent"]}% with {libraries["unobserved"]} unobserved stubs' if libraries["unobserved"] else ""
            print(f'libraries {libraries["done"]}/{libraries["total"]} ({libraries["percent"]}%){declared}')
            print(f'shaders {shaders["done"]}/{shaders["total"]} ({shaders["percent"]}%)')
        return 0
    finally:
        PRX, OPCODES, OBSERVED, INVENTORY = previous


if __name__ == "__main__":
    raise SystemExit(main())
