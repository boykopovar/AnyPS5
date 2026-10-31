import ast
import json
import operator
from pathlib import Path
import re
import sys


source = Path(sys.argv[1])
destination = Path(sys.argv[2])
macros = {}
operations = {ast.BitOr: operator.or_, ast.BitAnd: operator.and_, ast.LShift: operator.lshift,
              ast.RShift: operator.rshift, ast.Add: operator.add, ast.Sub: operator.sub}


def uncomment(text):
    return re.sub(r"/\*.*?\*/", "", text, flags=re.S)


for header in sorted(source.rglob("*.h")):
    for name, value in re.findall(r"^#\s*define\s+(\w+)\s+([^\n]+)", uncomment(header.read_text(encoding="utf-8")), re.M):
        if re.fullmatch(r"ERR_(?:LIB|R)_\w+|\w+_[RF]_\w+", name):
            if name in macros and macros[name] != value.strip():
                raise ValueError(f"Conflicting definition: {name}")
            macros[name] = value.strip()


def evaluate(expression):
    def visit(node):
        if isinstance(node, ast.Constant) and type(node.value) is int:
            return node.value
        if isinstance(node, ast.Name):
            return evaluate(macros[node.id])
        if isinstance(node, ast.BinOp) and type(node.op) in operations:
            return operations[type(node.op)](visit(node.left), visit(node.right))
        raise ValueError(f"Unsupported constant: {expression}")
    return visit(ast.parse(expression, mode="eval").body)


def pack(library, function, reason):
    return ((library & 255) << 24) | ((function & 4095) << 12) | (reason & 4095)


def code(expression, library):
    call = re.fullmatch(r"ERR_(PACK|FUNC|REASON)\((.*)\)", expression.strip())
    if call:
        values = [evaluate(value.strip()) for value in call[2].split(",")]
        if call[1] == "PACK":
            return pack(*values)
        if call[1] == "FUNC":
            return pack(library, values[0], 0)
        return pack(library, 0, values[0])
    return evaluate(expression.strip())


loader = uncomment((source / "err/err_all.c").read_text(encoding="utf-8"))
loaded = set(re.findall(r"\bERR_load_(\w+)_strings\(\);", loader)) - {"ERR"}
strings = {}
licenses = []
found = set()
for path in sorted(source.rglob("*.c")):
    text = path.read_text(encoding="latin-1")
    clean = uncomment(text)
    library = 0
    if path != source / "err/err.c":
        names = set(re.findall(r"\bERR_load_(\w+)_strings\(void\)", clean)) & loaded
        if not names:
            continue
        found.update(names)
        library_name = re.search(r"#define ERR_FUNC\(func\)\s+ERR_PACK\((\w+),", clean)
        if not library_name:
            raise ValueError(f"Missing library: {path}")
        library = evaluate(library_name[1])
    licenses.append(text[:text.index("#include")])
    tables = re.findall(r"static ERR_STRING_DATA (\w+)\[\]\s*=\s*\{(.*?)\n\};", clean, re.S)
    if not tables:
        raise ValueError(f"Missing string tables: {path}")
    for name, body in tables:
        entries = re.findall(r'\{\s*([^{}]+?),\s*("(?:[^"\\]|\\.)*"|NULL)\s*\}', body, re.S)
        remainder = re.sub(r'\{\s*([^{}]+?),\s*("(?:[^"\\]|\\.)*"|NULL)\s*\}', "", body, flags=re.S)
        if remainder.strip(" \t\r\n,"):
            raise ValueError(f"Unsupported table: {path}: {name}")
        for expression, message in entries:
            value = code(expression, library)
            if value == 0 and message == "NULL":
                continue
            if message == "NULL":
                raise ValueError(f"Missing message: {path}: {expression}")
            if name == "ERR_str_functs":
                value |= pack(evaluate("ERR_LIB_SYS"), 0, 0)
            strings[value] = json.loads(message)
if found != loaded:
    raise ValueError(f"Missing libraries: {loaded - found}")
destination.parent.mkdir(parents=True, exist_ok=True)
destination.write_text("\n".join(licenses) + "\nconstexpr ErrorString CryptoStrings[] = {\n" +
                       "".join(f"    {{0x{value:08X}, {json.dumps(message)}}},\n"
                               for value, message in sorted(strings.items())) + "};\n", encoding="utf-8")
