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

ROOT = Path(__file__).resolve().parent.parent
PRX = ROOT / "core" / "libs" / "prx"
OPCODES = ROOT / "core" / "shader" / "recompiler" / "RdnaDecoder" / "include" / "RdnaDecoder" / "RdnaOpcode.hpp"
ISA = Path(__file__).resolve().parent / "rdna_isa.txt"
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
PANEL_WIDTH, GAP, MAP_HEIGHT, HEADER = 495, 10, 280, 30
DONE_COLOR, TODO_COLOR, BORDER, TEXT = "#2ea043", "#6e7681", "#0d1117", "#ffffff"


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


def scan_library(path):
    done, todo, inert, live = set(), set(), set(), set()
    texts = [code(source.read_text(encoding="utf-8", errors="ignore")) for source in path.rglob("*.cpp")
             if "tests" not in source.relative_to(path).parts]
    exported = {match.group(1) for text in texts for match in EXPORT.finditer(text)}
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
                todo.add(name)
                continue
            done.add(name)
            (inert if inert_body(body, wrappers) else live).add(name)
    todo -= done
    inert -= live
    group = {"name": path.name, "label": path.name.removeprefix("libSce"), "done": len(done), "todo": len(todo),
             "done_names": sorted(done), "todo_names": sorted(todo), "inert_names": sorted(inert)}
    if not (done or todo):
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
            name, encoding = line.split()
            isa[name] = encoding
    by_camel = {camel(name): name for name in isa}
    enum = re.search(r"enum class RdnaOpcode[^{]*\{(.*?)\};", OPCODES.read_text(encoding="utf-8", errors="ignore"), re.S).group(1)
    opcodes = [o for o in re.findall(r"^\s*([A-Z]\w*)\s*[,=]", enum, re.M) if o not in OPCODE_SENTINELS]
    supported, extra = set(), []
    for opcode in opcodes:
        supported.update(name for name in OPCODE_VARIANTS.get(opcode, ()) if name in isa)
        name = OPCODE_ALIASES.get(opcode) or by_camel.get(opcode)
        if name in isa:
            supported.add(name)
            if name.startswith("FLAT_"):
                supported.update(n for n in (s + name.removeprefix("FLAT_") for s in FLAT_SEGMENTS) if n in isa)
        else:
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


def rect(x, y, w, h, color, stroke=0.5):
    return (f'<rect x="{x:.2f}" y="{y:.2f}" width="{w:.2f}" height="{h:.2f}" '
            f'fill="{color}" stroke="{BORDER}" stroke-width="{stroke}"/>')


def text(x, y, value, size):
    return (f'<text x="{x:.2f}" y="{y:.2f}" font-family="sans-serif" font-size="{size}" fill="{TEXT}" '
            f'stroke="{BORDER}" stroke-width="3" paint-order="stroke">{escape(value)}</text>')


def treemap(title, data, left):
    groups = sorted((g for g in data["groups"] if g["done"] + g["todo"]), key=lambda g: -(g["done"] + g["todo"]))
    parts = [text(left + 4, 21, f'{title}: {data["percent"]}% ({data["done"]}/{data["total"]})', 16)]
    for group, (x, y, w, h) in zip(groups, squarify([g["done"] + g["todo"] for g in groups], left, HEADER, PANEL_WIDTH, MAP_HEIGHT)):
        total = group["done"] + group["todo"]
        shared = [g["name"] for g in data["groups"] if g.get("shared_sources") == group["name"]]
        suffix = "; shared by " + ", ".join(shared) if shared else ""
        parts.append(f'<g><title>{escape(group["name"])}: {group["done"]}/{total} ({100 * group["done"] / total:.0f}%){escape(suffix)}</title>')
        for i, cell in enumerate(cells(total, x + 1, y + 1, w - 2, h - 2)):
            parts.append(rect(*cell, DONE_COLOR if i < group["done"] else TODO_COLOR))
        parts.append(f'<rect x="{x:.2f}" y="{y:.2f}" width="{w:.2f}" height="{h:.2f}" fill="none" stroke="{BORDER}" stroke-width="2"/>')
        label_length = int((w - 10) / 7)
        label = group["label"][:label_length] if label_length > 0 else ""
        if (label == group["label"] or len(label) >= 3) and h > 22:
            parts.append(text(x + 5, y + 16, label, 12))
        parts.append("</g>")
    return parts


def render(libraries, shaders):
    width, height = 2 * PANEL_WIDTH + GAP, HEADER + MAP_HEIGHT
    parts = [f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {width} {height}" width="{width}" height="{height}">',
             rect(0, 0, width, height, BORDER, 0)]
    parts += treemap("System libraries*", libraries, 0)
    parts += treemap("GPU shader instructions", shaders, PANEL_WIDTH + GAP)
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
        f'<p>Generated by <a href="{SOURCE}/tools/progress.py">tools/progress.py</a> on every push to <code>main</code>. '
        'Raw numbers: <a href="progress.json">progress.json</a>.</p>',
        table("System libraries", "Library", libraries),
        "<p>A function is implemented when it no longer calls <code>NotImplemented_nid_no_patch</code>; "
        "comparison reports separately mark implemented functions whose body only returns a constant or throws. "
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
    changes = f', {", ".join(counts)} {unit}' if counts else ""
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
        delta = "n/a"
        if old_percent is not None and new_percent is not None:
            delta = f'{round(new_percent - old_percent, 2) or 0:+.2f} pp'
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
    return "\n".join(lines)


SITE_REPORTS = ("progress.json", "badge-libraries.svg", "badge-shaders.svg", "progress.svg", "progress.html")


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
    (output / "badge-libraries.svg").write_text(badge("libraries*", libraries), encoding="utf-8")
    (output / "badge-shaders.svg").write_text(badge("shaders", shaders), encoding="utf-8")
    (output / "progress.svg").write_text(render(libraries, shaders), encoding="utf-8")
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
        for index, group in enumerate(groups):
            location = f"{section}.groups[{index}]"
            if not isinstance(group, dict) or not isinstance(group.get("name"), str):
                raise ValueError(f"{location} must be an object with a string name")
            if "shared_sources" in group and not isinstance(group["shared_sources"], str):
                raise ValueError(f"{location}.shared_sources must be a string")
            if section == "libraries":
                for field in ("done", "todo"):
                    if field in group and (type(group[field]) is not int or group[field] < 0):
                        raise ValueError(f"{location}.{field} must be a non-negative integer")
            for field in ("done_names", "todo_names", "inert_names"):
                values = group.get(field, [])
                if not isinstance(values, list) or any(not isinstance(value, str) for value in values):
                    raise ValueError(f"{location}.{field} must be a list of strings")


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
                except (OSError, ValueError) as error:
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
        try:
            libraries, shaders = collect_libraries(), collect_shaders()
        except FileNotFoundError as error:
            parser.error(f"{args.root or ROOT} is not a source tree: {error.filename} not found")
        try:
            libraries = select_libraries(libraries, args.module or [])
        except ValueError as error:
            parser.error(str(error))
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
            print(f'libraries {libraries["done"]}/{libraries["total"]} ({libraries["percent"]}%)')
            print(f'shaders {shaders["done"]}/{shaders["total"]} ({shaders["percent"]}%)')
        return 0
    finally:
        PRX, OPCODES = previous


if __name__ == "__main__":
    raise SystemExit(main())
