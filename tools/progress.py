import argparse
import json
import os
import re
from html import escape
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
PRX = ROOT / "core" / "libs" / "prx"
OPCODES = ROOT / "core" / "shader" / "recompiler" / "RdnaDecoder" / "include" / "RdnaDecoder" / "RdnaOpcode.hpp"
ISA = Path(__file__).resolve().parent / "rdna_isa.txt"
SOURCE = f'https://github.com/{os.environ.get("GITHUB_REPOSITORY", "boykopovar/AnyPS5")}/blob/main'
DEFINITION = re.compile(r"\bAPS5_VABI\s+(\w+)\s*\([^;{]*\)\s*(?:noexcept\s*)?\{")
STUB = "NotImplemented_nid_no_patch"
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
PANEL_WIDTH, GAP, MAP_HEIGHT, HEADER = 495, 10, 280, 30
DONE_COLOR, TODO_COLOR, BORDER, TEXT = "#2ea043", "#1f6feb", "#0d1117", "#ffffff"


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


def scan_library(path):
    done, todo = set(), set()
    for source in path.rglob("*.cpp"):
        text = source.read_text(errors="ignore")
        for match in DEFINITION.finditer(text):
            name = match.group(1)
            if name.endswith("_nid_no_patch"):
                continue
            body = text[match.end() - 1:body_end(text, match.end() - 1)]
            (todo if STUB in body else done).add(name)
    todo -= done
    return {"name": path.name, "label": path.name.removeprefix("libSce"), "done": len(done), "todo": len(todo)}


def summarize(groups):
    done = sum(g["done"] for g in groups)
    total = done + sum(g["todo"] for g in groups)
    return {"done": done, "total": total, "percent": round(100 * done / total, 2) if total else 0, "groups": groups}


def collect_libraries():
    return summarize([scan_library(p) for p in sorted(PRX.iterdir()) if p.is_dir()])


def camel(name):
    return "".join(part.capitalize() for part in name.split("_"))


def collect_shaders():
    isa = {}
    for line in ISA.read_text().splitlines():
        if line and not line.startswith("#"):
            name, encoding = line.split()
            isa[name] = encoding
    by_camel = {camel(name): name for name in isa}
    enum = re.search(r"enum class RdnaOpcode[^{]*\{(.*?)\};", OPCODES.read_text(), re.S).group(1)
    opcodes = [o for o in re.findall(r"^\s*([A-Z]\w*)\s*[,=]", enum, re.M) if o not in OPCODE_SENTINELS]
    supported, extra = set(), []
    for opcode in opcodes:
        name = OPCODE_ALIASES.get(opcode) or by_camel.get(opcode)
        if name in isa:
            supported.add(name)
            if name.startswith("FLAT_"):
                supported.update(n for n in (s + name.removeprefix("FLAT_") for s in FLAT_SEGMENTS) if n in isa)
        else:
            extra.append(opcode)
    groups = {}
    for name, encoding in isa.items():
        group = groups.setdefault(encoding, {"name": encoding, "label": encoding, "done": 0, "todo": 0})
        group["done" if name in supported else "todo"] += 1
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
    if not count:
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
        parts.append(f'<g><title>{escape(group["name"])}: {group["done"]}/{total} ({100 * group["done"] / total:.0f}%)</title>')
        for i, cell in enumerate(cells(total, x + 1, y + 1, w - 2, h - 2)):
            parts.append(rect(*cell, DONE_COLOR if i < group["done"] else TODO_COLOR))
        parts.append(f'<rect x="{x:.2f}" y="{y:.2f}" width="{w:.2f}" height="{h:.2f}" fill="none" stroke="{BORDER}" stroke-width="2"/>')
        label = group["label"][:int((w - 10) / 7)]
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
    lines = [f"## {heading}", "", f"| {column} | Implemented | Total | % |", "|---|---:|---:|---:|"]
    for group in sorted(data["groups"], key=lambda g: g["name"].lower()):
        total = group["done"] + group["todo"]
        percent = f'{100 * group["done"] / total:.0f}%' if total else "-"
        lines.append(f'| {group["name"]} | {group["done"]} | {total} | {percent} |')
    lines.append(f'| **Total** | **{data["done"]}** | **{data["total"]}** | **{data["percent"]}%** |')
    return "\n".join(lines)


def summary(libraries, shaders):
    extra = ", ".join(f"`{o}`" for o in shaders["extra"])
    return "\n\n".join([
        "# Progress",
        "![progress](progress.svg)",
        f"Generated by [tools/progress.py]({SOURCE}/tools/progress.py) on every push to `main`.",
        table("System libraries", "Library", libraries),
        "A function is implemented when it no longer calls `NotImplemented_nid_no_patch`. "
        f"The total only includes functions already declared in [core/libs/prx]({SOURCE}/core/libs/prx), "
        "not every function exported by the PS5 firmware.",
        table("GPU shader instructions", "Encoding", shaders),
        f"The total is the AMD RDNA 1 + RDNA 2 instruction list ([tools/rdna_isa.txt]({SOURCE}/tools/rdna_isa.txt)). "
        f"An instruction is implemented when the [decoder]({SOURCE}/core/shader/recompiler/RdnaDecoder) recognizes it. "
        f"Decoded opcodes not present in AMD's public list are not counted: {extra}.",
    ]) + "\n"


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("output", type=Path)
    output = parser.parse_args().output
    output.mkdir(parents=True, exist_ok=True)
    libraries, shaders = collect_libraries(), collect_shaders()
    (output / "progress.json").write_text(json.dumps({"libraries": libraries, "shaders": shaders}, indent=2))
    (output / "badge-libraries.svg").write_text(badge("libraries*", libraries))
    (output / "badge-shaders.svg").write_text(badge("shaders", shaders))
    (output / "progress.svg").write_text(render(libraries, shaders))
    (output / "README.md").write_text(summary(libraries, shaders))
    print(f'libraries {libraries["done"]}/{libraries["total"]} ({libraries["percent"]}%)')
    print(f'shaders {shaders["done"]}/{shaders["total"]} ({shaders["percent"]}%)')
