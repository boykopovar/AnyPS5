import argparse
import io
import itertools
import json
import os
import posixpath
import re
import subprocess
import sys
import tokenize
from collections import defaultdict
from pathlib import Path, PurePosixPath
from urllib.parse import unquote

sys.path.insert(0, str(Path(__file__).resolve().parent))
from nid_names import CPP_TOKEN, NID_POSTFIX, compute_nid, source_aliases
import env_reference

RULES = {
    "comment": ("error", "docs/dev/CONVENTIONS.md", "Comments are only for technical debt, so they need a change to docs/dev/TechnicalDebt.md or docs/dev/debt/ in the same pull request; #endif and namespace ends are always allowed"),
    "silent-stub": ("error", "CONTRIBUTING.md#code", "Unimplemented exports call NotImplemented_nid_no_patch(__func__); silent stubs are listed under Silent stubs in docs/dev/debt/<library>.md, by their file or by name with a path in their library"),
    "debt-file": ("error", "CONTRIBUTING.md#documentation", "Technical debt about one library or component goes in docs/dev/debt/<name>.md, named after the first core/ path it links: <library> for core/libs/prx/<library>, otherwise <component> for core/<component>; entries that link neither stay in docs/dev/TechnicalDebt.md"),
    "duplicate-export": ("error", "CONTRIBUTING.md#code", "Already exported by another library: remove the copy, or use APS5_DUMMY_FUN if nothing is left"),
    "unknown-nid": ("error", "CONTRIBUTING.md#code", "An Unknown export has the NID of a symbol in core/libs/prx, so the NID is not unknown: name the export after that symbol, or give an AnyPS5 helper the _nid_no_patch suffix"),
    "system-dependency": ("error", "CONTRIBUTING.md#code", "Third-party code is a submodule under 3rdparty/ built from source, not found on the system"),
    "title-specific": ("error", "CONTRIBUTING.md#code", "Implement the general behaviour, not what one title needs; title-specific code belongs in its sce_module"),
    "extension": ("error", "CONTRIBUTING.md#code", "Avoid non-standard extensions where standard C++ is enough"),
    "alignment": ("error", "CONTRIBUTING.md#code", "MinGW GCC rejects static objects aligned above 8192 bytes; align a larger block at run time inside a bigger buffer"),
    "notes-file": ("error", "CONTRIBUTING.md#branches-and-pull-requests", "Notes, investigation and agent files go in the pull request, not in the repository; only the root AGENTS.md is allowed"),
    "binary": ("error", "docs/dev/CONVENTIONS.md", "Only UTF-8 text files in the repository; images go in the gist comments"),
    "ci-gate": ("error", "CONTRIBUTING.md#build-and-test", "A job that runs for pull requests needs a file filter: paths on the pull_request trigger, or needs eligibility and an if on its outputs"),
    "doc-link": ("error", "CONTRIBUTING.md#documentation", "Relative link to a file that does not exist"),
    "commit-subject": ("error", "docs/dev/CONVENTIONS.md", "Commit subject is not Conventional Commits"),
    "pr-title": ("error", "docs/dev/CONVENTIONS.md", "Pull request title is not Conventional Commits"),
    "pr-template": ("error", ".github/pull_request_template.md", "Fill in the pull request template"),
    "env-reference": ("error", "docs/dev/ENVIRONMENT.md", "Every APS5_* or ANYPS5_* name a pull request adds to the code has a row in docs/dev/ENVIRONMENT.md, a row whose name the pull request stops reading is removed, and a row the pull request removes is no longer read"),
    "debt-length": ("warning", "CONTRIBUTING.md#documentation", "Technical debt entry over 1000 characters: state the gap first (what differs from the console, what is unverified, what throws) and keep only the behaviour it needs"),
}

CPP = {".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx", ".inl", ".ipp"}
IMAGES = {".png", ".jpg", ".jpeg", ".gif", ".bmp", ".svg", ".webp", ".ico", ".tga", ".dds"}
NOTES = {".md", ".markdown", ".rst", ".log", ".patch", ".diff"}
AGENT_FILES = {
    "agents.md", "claude.md", "gemini.md", ".cursorrules", "copilot-instructions.md",
    ".windsurfrules", "windsurf.md", "chatgpt.md", ".chatgptrules", ".aider.conf.yml",
}
DEBT = "docs/dev/TechnicalDebt.md"
DEBT_DIR = "docs/dev/debt/"
DEBT_TARGET = re.compile(r"\]\((?:\.\./)+(core/[^)#\s]+)")
DEBT_ENTRY_LIMIT = 1000

CONVENTIONAL = re.compile(r"^(feat|fix|docs|style|refactor|perf|test|build|ci|chore|revert)(\([^)]+\))?!?: \S")
STRINGS = re.compile(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'')
ALLOWED_COMMENT = re.compile(r"^\s*(#\s*endif\b|}\s*;?\s*//\s*(end\s+)?namespace\b)")
CMAKE_TOKEN = re.compile(r'\[(=*)\[.*?\]\1\]|"(?:\\.|[^"\\])*"|(?P<comment>#\[(?P<level>=*)\[.*?\](?P=level)\]|#[^\n]*)|\\.', re.S)
EXPORT = re.compile(r"\bAPS5_VABI\s+(\w+)\s*\(")
IDENTIFIER = re.compile(r"[A-Za-z_][A-Za-z0-9_]*")
NID_SOURCES = r"APS5_EXPORT|APS5_VABI|_nid_postfix|ASM_FUNCTION"
STUB_BODY = re.compile(r"^(?:\(void\)\s*\w+\s*;|static_cast<void>\(\s*\w+\s*\)\s*;)*"
                       r"(?:return\s*\(?\s*(?:-?(?:0x[0-9a-fA-F]+|\d+)[uUlL]*|nullptr|true|false|\w*_OK)?\s*\)?\s*;)?$")
SYSTEM_DEPENDENCY = re.compile(r"\b(find_package\s*\((?!\s*(Python3|Threads|Git)\b)|pkg_check_modules\b|pkg_search_module\b|find_(library|path|program|file)\b)", re.I)
TITLE = re.compile(r"\b(PPSA|CUSA)\d{5}\b")
EXTENSION = re.compile(r"__attribute__|__declspec|#\s*pragma\s+(?!once\b)")
INTEGER = r"(0[xX][0-9a-fA-F']+|0[bB][01']+|[0-9][0-9']*)[uUlLzZ]*"
ALIGNAS = re.compile(rf"\balignas\s*\(\s*(?:{INTEGER}|([A-Za-z_][A-Za-z0-9_]*))\s*\)")
CONSTANT = re.compile(rf"\bconstexpr\b[^;=]*?\b([A-Za-z_][A-Za-z0-9_]*)\s*(?:=\s*|\{{\s*){INTEGER}\s*[;}}]")
NAMESPACE = re.compile(r"\bnamespace\s+((?:[A-Za-z_][A-Za-z0-9_]*\s*::\s*(?:inline\s+)?)*[A-Za-z_][A-Za-z0-9_]*)\s*$|\bnamespace\s*$")
LINKAGE = re.compile(r'\bextern\s*""\s*$')
ALIGNMENT_LIMIT = 8192
LINK = re.compile(r"\[[^\]]+\]\(([^)\s]+)")
FENCE = re.compile(r"^ {0,3}(```|~~~).*?^ {0,3}\1[ \t]*\r?$|^ {0,3}(?:```|~~~).*\Z", re.S | re.M)
INLINE = re.compile(r"`[^`\n]*`")
SECTION = re.compile(r"^###(?!#)\s*(.+?)\s*$", re.M)
KEY = re.compile(r"^( *)([A-Za-z0-9_-]+|'[^']*'|\"[^\"]*\"):(?:\s+(.*?))?\s*$")



def git(*args, text=True):
    try:
        result = subprocess.run(
            ["git", "-c", "core.quotePath=false", *args],
            capture_output=True,
            check=True,
        )
    except subprocess.CalledProcessError as error:
        message = error.stderr.decode("utf-8", errors="replace").strip()
        print(
            f"Git command failed: {message or 'unknown Git error'}",
            file=sys.stderr,
        )
        raise SystemExit(2) from None
    except FileNotFoundError:
        print("Git executable not found. Please install Git.", file=sys.stderr)
        raise SystemExit(2) from None

    output = result.stdout
    return output.decode("utf-8", errors="replace") if text else output



class Check:
    def __init__(self, base, head):
        self.head = head
        self.target = base
        self.base = git("merge-base", base, head).strip()
        self.findings = []
        self.added = defaultdict(list)
        self.status = {}
        self.numstat = {}
        entries = [(metadata.split(), path) for metadata, path in
                   (entry.split("\t", 1) for entry in git("ls-tree", "-rz", "--full-tree", head).split("\0") if entry)]
        self.modes = {path: fields[0] for fields, path in entries}
        self.objects = {path: fields[2] for fields, path in entries if fields[1] == "blob"}
        self.blobs = {}
        self.tree = set(self.modes)
        self.dirs = {str(parent) for path in self.tree for parent in PurePosixPath(path).parents}
        diff = ("--find-renames", "-l0", self.base, head)
        statuses = iter(git("diff", "--name-status", "-z", *diff).split("\0")[:-1])
        for status in statuses:
            path = next(statuses)
            if status.startswith("R"):
                path = next(statuses)
            self.status[path] = status[0]
        statistics = iter(git("diff", "--numstat", "-z", *diff).split("\0")[:-1])
        for line in statistics:
            added, deleted, path = line.split("\t", 2)
            if not path:
                next(statistics)
                path = next(statistics)
            self.numstat[path] = (added, deleted)
        paths = iter([path for path, status in self.status.items() for _ in range(2 if status == "T" else 1)])
        path, number = None, None
        for line in git("diff", "-U0", "--no-color", *diff).split("\n"):
            if line.startswith("diff --git "):
                path, number = next(paths), None
            elif line.startswith("@@"):
                number = int(re.search(r"\+(\d+)", line).group(1))
            elif path and number is not None and line.startswith("+"):
                self.added[path].append((number, line[1:]))
                number += 1
        self.read([path for path in sorted(self.tree) if is_debt(path)] +
                  [path for path, (added, _) in self.numstat.items() if self.status.get(path) != "D" and added != "-" and
                   not path.startswith("3rdparty/") and PurePosixPath(path).suffix.lower() not in IMAGES])

    def report(self, rule, path, line, detail=""):
        self.findings.append((rule, path, line, detail))

    def files(self, predicate):
        return [(path, lines) for path, lines in self.added.items() if not path.startswith("3rdparty/") and predicate(PurePosixPath(path))]

    def read(self, paths):
        pending = [path for path in dict.fromkeys(paths) if path in self.objects and path not in self.blobs]
        if not pending:
            return
        data = subprocess.run(["git", "cat-file", "--batch"], input="".join(f"{self.objects[path]}\n" for path in pending).encode(),
                              capture_output=True, check=True).stdout
        offset = 0
        for path in pending:
            end = data.index(b"\n", offset)
            header = data[offset:end].split()
            offset = end + 1
            if len(header) == 3 and header[1] == b"blob":
                size = int(header[2])
                self.blobs[path] = data[offset:offset + size]
                offset += size + 1

    def blob(self, path):
        data = self.blobs.get(path)
        return git("show", f"{self.head}:{path}", text=False) if data is None else data

    def show(self, path):
        return self.blob(path).decode("utf-8", errors="replace")

    def code(self):
        debt = [entry for path in sorted(self.tree) if is_debt(path) for entry in silent_stub_debt(self.show(path), path)]
        exports = {}
        unknowns, symbols = [], []
        for path, lines in self.files(lambda p: p.suffix in CPP):
            core = path.startswith("core/") and "tests" not in PurePosixPath(path).parts
            source = self.show(path).split("\n")
            masked = cpp_text("\n".join(source), preserve_comments=True).split("\n")
            assembly = asm_exports("\n".join(source)) if core and path.startswith("core/libs/prx/") else {}
            constants, starts = None, None
            for number, text in lines:
                bare = masked[number - 1]
                if ("//" in bare or "/*" in bare) and not ALLOWED_COMMENT.match(bare):
                    self.report("comment", path, number)
                if EXTENSION.search(bare):
                    self.report("extension", path, number, EXTENSION.search(bare).group(0))
                if "alignas" in bare:
                    if constants is None:
                        constants = named_constants(cpp_text("\n".join(source)))
                        starts = list(itertools.accumulate((len(line) + 1 for line in masked), initial=0))
                    alignment = over_aligned(bare, constants, starts[number - 1])
                    if alignment:
                        self.report("alignment", path, number, alignment)
                if core and TITLE.search(text):
                    self.report("title-specific", path, number, TITLE.search(text).group(0))
                if core and path.startswith("core/libs/prx/"):
                    unknowns.extend((nid, name, path, number) for nid, name in source_aliases(text) if "unknown" in name.lower())
                    symbols.extend((symbol, path, number) for symbol in nid_symbols(text))
                exports.update((name, (path, number)) for name in assembly.get(number, ()) if not name.endswith(("_nid_no_patch", "_nid_no_patch_cut")))
                match = EXPORT.search(bare)
                if match and core and path.startswith("core/libs/prx/") and not match.group(1).endswith(("_nid_no_patch", "_nid_no_patch_cut")):
                    source = source or self.show(path).splitlines()
                    body = function_body(source, number - 1)
                    if body is None:
                        continue
                    exports[match.group(1)] = (path, number)
                    if match.group(1).startswith("sce") and STUB_BODY.match(body) and not listed(debt, match.group(1), path):
                        self.report("silent-stub", path, number, match.group(1))
        self.duplicates(exports)
        self.unknown_nids(unknowns, symbols)
        for path, lines in self.files(lambda p: p.suffix in (".py", ".cmake") or p.name == "CMakeLists.txt"):
            source = self.show(path)
            comments = python_comments(source) if path.endswith(".py") else cmake_comments(source)
            for number, text in lines:
                shebang = text.startswith("#!") or (number == 1 and text.startswith("\ufeff#!"))
                comment = number in comments if comments is not None else "#" in STRINGS.sub('""', text)
                if comment and not shebang:
                    self.report("comment", path, number)
        if any(is_debt(path) for path in self.status):
            self.findings = [finding for finding in self.findings if finding[0] != "comment"]

    def duplicates(self, exports):
        if not exports:
            return
        paths = sorted(path for path in self.objects if path.startswith("core/libs/prx/") and
                       "tests" not in PurePosixPath(path).parts)
        self.read(paths)
        for path in paths:
            text = self.show(path)
            if "APS5_VABI" not in text and "ASM_FUNCTION" not in text:
                continue
            source = cpp_text(text).split("\n")
            assembly = asm_exports(text)
            for number, line in enumerate(source):
                names = [match.group(1) for match in EXPORT.finditer(line)
                         if match.group(1) in exports and library(path) != library(exports[match.group(1)][0])
                         and function_body(source, number) is not None]
                names += [name for name in assembly.get(number + 1, ()) if name in exports and library(path) != library(exports[name][0])]
                for name in names:
                    own, own_number = exports[name]
                    self.report("duplicate-export", own, own_number, f"{name} is also in {library(path)}")

    def unknown_nids(self, unknowns, symbols):
        if not unknowns and not symbols:
            return
        result = subprocess.run(["git", "grep", "-nE", "--full-name", NID_SOURCES, self.head, "--", ":(top)core/libs/prx"], capture_output=True, encoding="utf-8", errors="replace")
        tree_unknowns, tree_symbols = defaultdict(list), defaultdict(list)
        for path, number, text in (line.split(":", 3)[1:] for line in filter(None, result.stdout.split("\n"))):
            if "tests" in PurePosixPath(path).parts or PurePosixPath(path).suffix not in CPP:
                continue
            for nid, name in source_aliases(text):
                if "unknown" in name.lower():
                    tree_unknowns[nid].append(f"{name} at {path}:{number}")
            for symbol in nid_symbols(text):
                tree_symbols[symbol_nid(symbol)].append(f"{symbol} at {path}:{number}")
        for nid, name, path, number in unknowns:
            if tree_symbols[nid]:
                self.report("unknown-nid", path, number, f"{nid} ({name}) is {tree_symbols[nid][0]}")
        for symbol, path, number in symbols:
            nid = symbol_nid(symbol)
            if tree_unknowns[nid]:
                self.report("unknown-nid", path, number, f"{symbol} has the NID {nid} of {tree_unknowns[nid][0]}")

    def build(self):
        for path, lines in self.files(lambda p: p.name == "CMakeLists.txt" or p.suffix == ".cmake"):
            if path == "cmake/SystemDeps.cmake":
                continue
            for number, text in lines:
                if SYSTEM_DEPENDENCY.search(text.split("#", 1)[0]):
                    self.report("system-dependency", path, number, text.strip())
        for number, text in self.added.get(".gitmodules", []):
            match = re.match(r"\s*path\s*=\s*(\S+)", text)
            if match and not match.group(1).startswith("3rdparty/"):
                self.report("system-dependency", ".gitmodules", number, match.group(1))

    def shaders(self):
        if any(is_debt(path) for path in self.status):
            return
        for path, lines in self.files(lambda p: p.suffix in (".comp", ".frag", ".vert", ".glsl")):
            masked = cpp_text(self.show(path), preserve_comments=True).split("\n")
            for number, _ in lines:
                bare = masked[number - 1]
                if ("//" in bare or "/*" in bare) and not ALLOWED_COMMENT.match(bare):
                    self.report("comment", path, number)

    def submodules(self):
        result = subprocess.run(["git", "config", "--no-includes", "-z", "--blob", f"{self.head}:.gitmodules", "--list"],
                                capture_output=True, encoding="utf-8", errors="replace")
        values = {key: value for key, _, value in (entry.partition("\n") for entry in result.stdout.split("\0") if entry)}
        return {path for key, path in values.items() if key.endswith(".path") and values.get(f"{key[:-5]}.url")}

    def repository(self):
        gitlinks = {path for path, mode in self.modes.items() if mode == "160000" and path.startswith("3rdparty/")}
        checked = gitlinks if ".gitmodules" in self.status else gitlinks & self.status.keys()
        if checked:
            for path in sorted(checked - self.submodules()):
                self.report("system-dependency", path, 0, "submodule without a url in .gitmodules")
        for path, (added, deleted) in self.numstat.items():
            if path.startswith("3rdparty/"):
                continue
            if self.modes.get(path) == "160000":
                self.report("system-dependency", path, 0, "submodule outside 3rdparty/")
                continue
            suffix = PurePosixPath(path).suffix.lower()
            if self.status.get(path) != "D" and (added == "-" or suffix in IMAGES or not utf8(self.blob(path))):
                self.report("binary", path, 0)
            if self.status.get(path) in ("A", "R") and allowed_notes(path) is False:
                self.report("notes-file", path, 0)

    def docs(self):
        files = dict(self.files(lambda p: p.suffix == ".md"))
        for path, status in self.status.items():
            if status == "R" and not path.startswith("3rdparty/") and PurePosixPath(path).suffix == ".md":
                files[path] = list(enumerate(self.show(path).split("\n"), 1))
        for path, lines in files.items():
            fenced = fenced_lines(self.show(path))
            for number, text in lines:
                if is_debt(path) and text.startswith("- "):
                    name = debt_name(text)
                    expected = f"{DEBT_DIR}{name}.md" if name else DEBT
                    if path != expected:
                        self.report("debt-file", path, number, expected)
                if number in fenced:
                    continue
                for target, resolved in links(path, text):
                    if resolved not in self.tree and resolved not in self.dirs:
                        self.report("doc-link", path, number, target)

    def debt_entries(self):
        for path, lines in self.files(lambda p: is_debt(p.as_posix())):
            for number, text in lines:
                if long_debt_entry(text):
                    self.report("debt-length", path, number, f"{len(text)} characters")

    def workflows(self):
        for path in sorted(self.status):
            if self.status[path] == "D" or not re.fullmatch(r"\.github/workflows/[^/]+\.ya?ml", path):
                continue
            source = self.show(path).splitlines()
            lines = [(number, KEY.match(re.sub(r"(^|\s)#.*", "", text))) for number, text in enumerate(source, 1)]
            lines = [(number, len(match[1]), match[2].strip("'\""), match[3] or "") for number, match in lines if match]
            triggers = [entry for entry in lines if entry[1] == 0 and entry[2] == "on"]
            if not triggers or pull_request_filtered(lines, triggers[0]):
                continue
            for number, name, needs, condition in jobs(lines, source):
                if name != "eligibility" and ("eligibility" not in needs or "needs.eligibility.outputs." not in condition):
                    self.report("ci-gate", path, number, name)

    def commits(self):
        for line in git("log", "--no-merges", "--format=%h %s", f"{self.base}..{self.head}").splitlines():
            sha, subject = line.split(" ", 1)
            if not CONVENTIONAL.match(subject):
                self.report("commit-subject", "", 0, f"{sha} {subject}")

    def env_reference(self):
        doc = "docs/dev/ENVIRONMENT.md"
        added, removed, dropped = {}, set(), set()
        path, number, in_hunk = "", 0, False
        for line in git("diff", "-U0", self.base, self.head, "--", ":(top)core", f":(top){doc}").splitlines():
            if line.startswith("diff --git"):
                path, in_hunk = "", False
            elif line.startswith("@@"):
                number = int(re.search(r"\+(\d+)", line).group(1)) - 1
                in_hunk = True
            elif in_hunk:
                if path == doc:
                    if line.startswith("-"):
                        dropped.update(env_reference.documented_pattern(env_reference.PREFIXES).findall(line[1:]))
                elif line.startswith("+"):
                    number += 1
                    for _, match in env_reference.line_reads(line[1:]):
                        added.setdefault(match.group("name"), (path, number))
                elif line.startswith("-"):
                    removed.update(match.group("name") for _, match in env_reference.line_reads(line[1:]))
            elif line.startswith("+++ b/"):
                path = line[6:]
        result = subprocess.run(["git", "grep", "-nE", "--full-name", env_reference.GREP, self.head, "--", ":(top)core"],
                                capture_output=True, encoding="utf-8", errors="replace")
        reads = set()
        for line in result.stdout.splitlines():
            reads.update(match.group("name") for _, match in env_reference.line_reads(line))
        pattern = env_reference.documented_pattern(env_reference.PREFIXES)
        documented = set(pattern.findall(self.show(doc))) if doc in self.tree else set()
        if git("ls-tree", "--full-tree", "--name-only", self.target, "--", doc).strip() == doc:
            documented |= set(pattern.findall(git("show", f"{self.target}:{doc}"))) - dropped
        for name, (source, line) in sorted(added.items()):
            if name not in documented:
                self.report("env-reference", source, line, f"{name} is read here but has no row in {doc}")
        for name in sorted(removed - reads):
            if name in documented:
                self.report("env-reference", doc, 0, f"{name} has a row but the code no longer reads it")
        for name in sorted(dropped & reads - documented):
            self.report("env-reference", doc, 0, f"{name} is still read by the code but this pull request removes its row")

    def pull_request(self, title, body):
        if not CONVENTIONAL.match(title):
            self.report("pr-title", "", 0, title)
        body = re.sub(r"<!--.*?-->", "", body or "", flags=re.S)
        sections = dict(zip(SECTION.findall(body), SECTION.split(body)[2::2]))
        for name in ("What", "Tested", "Checklist"):
            if name not in sections:
                found = re.search(rf"^[ \t]*#+[ \t]*{name}[ \t]*$", body, re.M | re.I)
                hint = f"; found '{found.group(0).strip()}'" if found else ""
                self.report("pr-template", "", 0, f"add the '### {name}' heading of the template{hint}")
            elif not sections[name].strip():
                self.report("pr-template", "", 0, f"fill the '### {name}' section")
        checklist = sections.get("Checklist", "")
        for item in re.findall(r"^\s*[-*+]\s*\[ \]\s*(.+)$", checklist, re.M):
            self.report("pr-template", "", 0, f"unchecked: {item.strip()}")
        if checklist and not re.search(r"AI-assisted:\s*(yes|no)\b", checklist, re.I):
            self.report("pr-template", "", 0, "answer AI-assisted with yes or no")
        if checklist and not re.search(r"Depends on:\s*(?:[-*+]\s+)?(#\d+|none)\b", checklist, re.I):
            self.report("pr-template", "", 0, "fill Depends on with #N or none")


def utf8(data):
    try:
        data.decode("utf-8")
    except UnicodeDecodeError:
        return False
    return True


def fenced_lines(text):
    inside = set()
    for match in FENCE.finditer(text):
        first = text.count("\n", 0, match.start()) + 1
        inside.update(range(first, first + match.group(0).count("\n") + 1))
    return inside


def links(path, text):
    result = []
    text = INLINE.sub("code", FENCE.sub("", text))
    for target in LINK.findall(text):
        if re.match(r"[A-Za-z][A-Za-z0-9+.-]*:|//|#", target):
            continue
        target = target.split("#", 1)[0].split("?", 1)[0]
        decoded = unquote(target)
        result.append((target, posixpath.normpath(decoded.lstrip("/") if decoded.startswith("/") else str(PurePosixPath(path).parent / decoded))))
    return result


def is_debt(path):
    return path == DEBT or (path.startswith(DEBT_DIR) and path.endswith(".md") and "/" not in path[len(DEBT_DIR):])


def long_debt_entry(text):
    return text.startswith("- ") and len(text) > DEBT_ENTRY_LIMIT


def debt_name(text):
    match = DEBT_TARGET.search(text)
    if not match:
        return None
    parts = match.group(1).split("/")
    return parts[3] if parts[1:3] == ["libs", "prx"] and len(parts) > 3 else parts[1]


def silent_stub_debt(text, path=DEBT):
    debt = []
    section_level = 0
    for line in text.splitlines():
        heading = re.match(r"^(#{1,6})\s+(.+?)\s*$", line)
        if heading:
            level = len(heading[1])
            if heading[2] == "Silent stubs":
                section_level = level
            elif level <= section_level:
                section_level = 0
        if section_level:
            debt.append((line, links(path, line)))
    return debt


def block(lines, start):
    _, indent, _, _ = lines[start]
    end = start + 1
    while end < len(lines) and lines[end][1] > indent:
        end += 1
    return lines[start + 1:end]


def flow_mapping(value):
    tokens = re.findall(r'''"(?:\\.|[^"\\])*"|'(?:''|[^'])*'|[^{}\[\],:\s]+|[{}\[\],:]''', value)
    if not tokens or tokens[0] != "{" or tokens[-1] != "}":
        return {}
    result = {}
    stack = []
    start, colon = 1, None
    for index, token in enumerate(tokens):
        if token in ("{", "["):
            stack.append(token)
        elif token in ("}", "]"):
            if not stack or stack.pop() != ("{" if token == "}" else "["):
                return {}
            if not stack and index != len(tokens) - 1:
                return {}
        if len(stack) == 1 and token == ":" and colon is None:
            colon = index
        if (len(stack) == 1 and token == ",") or (not stack and token == "}"):
            if colon == start + 1:
                result[tokens[start].strip("'\"")] = " ".join(tokens[colon + 1:index])
            elif start != index:
                return {}
            start, colon = index + 1, None
    return result if not stack else {}


def pull_request_filtered(lines, trigger):
    _, _, _, value = trigger
    if value:
        if value.lstrip().startswith("{"):
            triggers = flow_mapping(value)
            return "pull_request" not in triggers or bool({"paths", "paths-ignore"} & flow_mapping(triggers["pull_request"]).keys())
        return "pull_request" not in re.findall(r"[A-Za-z_]+", value)
    triggers = block(lines, lines.index(trigger))
    for index, (_, indent, name, value) in enumerate(triggers):
        if name == "pull_request" and indent == triggers[0][1]:
            if value:
                return bool({"paths", "paths-ignore"} & flow_mapping(value).keys())
            return any(entry[2] in ("paths", "paths-ignore") for entry in block(triggers, index))
    return True


def field(source, entry):
    number, indent, _, value = entry
    for line in (line for line in source[number:] if line.strip()):
        if len(line) - len(line.lstrip(" ")) <= indent:
            break
        value += " " + line.strip()
    return value


def jobs(lines, source):
    top = [index for index, entry in enumerate(lines) if entry[1] == 0 and entry[2] == "jobs"]
    if not top:
        return []
    entries = block(lines, top[0])
    result = []
    for index, (number, indent, name, _) in enumerate(entries):
        if indent != entries[0][1]:
            continue
        children = block(entries, index)
        child = min((entry[1] for entry in children), default=indent + 2)
        fields = {entry[2]: field(source, entry) for entry in children if entry[1] == child}
        result.append((number, name, re.findall(r"[A-Za-z0-9_-]+", fields.get("needs", "")), fields.get("if", "")))
    return result


def listed(debt, name, path):
    own = PurePosixPath(path).parts[:4]
    for text, targets in debt:
        for _, resolved in targets:
            if resolved == path or (PurePosixPath(resolved).parts[:4] == own and re.search(rf"\b{name}\b", text)):
                return True
    return False


def library(path):
    return PurePosixPath(path).parts[3]


def symbol_nid(name):
    if name.endswith(("_nid_no_patch", "_nid_no_patch_cut")) or name.startswith("SDL_"):
        return None
    base = re.sub(r"_nid_disambig\d+$", "", name.removesuffix(NID_POSTFIX))
    return compute_nid(base) if base else None


def asm_exports(text):
    tokens = [match for match in CPP_TOKEN.finditer(text) if match.group("comment") is None]
    exports = defaultdict(list)
    for index, match in enumerate(tokens[:-2]):
        name = tokens[index + 2].group()[1:-1]
        if match.group() in ("APS5_ASM_FUNCTION", "APS5_SIGNAL_ASM_FUNCTION") and tokens[index + 1].group() == "(" and tokens[index + 2].group("string") is not None and IDENTIFIER.fullmatch(name):
            exports[text.count("\n", 0, tokens[index + 2].start()) + 1].append(name)
    return exports


def nid_symbols(text):
    tokens = [match for match in CPP_TOKEN.finditer(text) if match.group("comment") is None]
    names = []
    for index, match in enumerate(tokens):
        token, following = match.group(), tokens[index + 1:index + 3]
        if token == "APS5_VABI" and following and IDENTIFIER.fullmatch(following[0].group()):
            names.append(following[0].group())
        elif match.group("code") is not None and token.endswith(NID_POSTFIX):
            names.append(token)
        elif token in ("APS5_ASM_FUNCTION", "APS5_SIGNAL_ASM_FUNCTION") and len(following) == 2 and following[0].group() == "(" and following[1].group("string") is not None:
            names.append(following[1].group()[1:-1])
    return [name for name in dict.fromkeys(names) if symbol_nid(name)]


def allowed_notes(path):
    p = PurePosixPath(path)
    if p.name.lower() in AGENT_FILES:
        return path == "AGENTS.md"
    if p.suffix.lower() not in NOTES:
        return None
    return path in ("README.md", "CONTRIBUTING.md") or p.parts[0] == ".github" or p.parts[:2] in (("docs", "dev"), ("docs", "user"))


def python_comments(text):
    try:
        tokens = list(tokenize.generate_tokens(io.StringIO(text.removeprefix("\ufeff")).readline))
    except (tokenize.TokenError, SyntaxError):
        return None
    return {token.start[0] for token in tokens if token.type == tokenize.COMMENT}


def cmake_comments(text):
    lines = set()
    for match in CMAKE_TOKEN.finditer(text):
        if match.group("comment") is not None:
            first = text.count("\n", 0, match.start()) + 1
            lines.update(range(first, first + match.group().count("\n") + 1))
    return lines


def cpp_text(text, preserve_comments=False):
    def mask(match):
        if match.group("code") is not None or preserve_comments and match.group("comment") is not None:
            return match.group()
        blank = re.sub(r"[^\n]", " ", match.group())
        return blank if match.group("comment") is not None else '""' + blank[2:]

    return CPP_TOKEN.sub(mask, text)


def integer_literal(text):
    digits = text.replace("'", "").lower()
    if digits.startswith(("0x", "0b")):
        return int(digits, 0)
    return int(digits, 8 if len(digits) > 1 and digits[0] == "0" else 10)


def brace_scopes(text):
    stack, scopes, boundary = [], [], -1
    for index, character in enumerate(text):
        if character not in "{};":
            continue
        if character == "{":
            head = text[boundary + 1:index]
            outer = next((key[1] for _, key in reversed(stack) if key and key[0] == "namespace"), "")
            match = NAMESPACE.search(head)
            if match and re.search(r"\binline\s+namespace\b", head):
                key = ("namespace", outer)
            elif match:
                key = ("namespace", outer + "::" + re.sub(r"\binline\b|\s+", "", match.group(1) or "{anonymous}"))
            else:
                key = None if LINKAGE.search(head) else ("block", index)
            stack.append((index, key))
        elif character == "}" and stack:
            start, key = stack.pop()
            scopes.append((start, index, key))
        boundary = index
    return scopes


def scope_chain(scopes, position):
    chain = []
    for _, _, key in sorted((scope for scope in scopes if scope[0] < position < scope[1]), reverse=True):
        if key and key[0] == "namespace":
            parts = key[1].split("::")
            chain.extend(prefix for prefix in (("namespace", "::".join(parts[:count])) for count in range(len(parts), 1, -1)) if prefix not in chain)
        elif key:
            chain.append(key)
    return chain + [("namespace", "")]


def named_constants(text):
    scopes = brace_scopes(text)
    definitions = [(match.group(1), integer_literal(match.group(2)), match.start(), scope_chain(scopes, match.start())[0]) for match in CONSTANT.finditer(text)]
    return definitions, scopes


def constant_value(constants, name, position):
    definitions, scopes = constants
    chain = scope_chain(scopes, position)
    visible = [(-chain.index(scope), start, value) for found, value, start, scope in definitions if found == name and start < position and scope in chain]
    return max(visible)[2] if visible else 0


def over_aligned(text, constants=((), ()), offset=0):
    for match in ALIGNAS.finditer(text):
        value = integer_literal(match.group(1)) if match.group(1) else constant_value(constants, match.group(2), offset + match.start())
        if value > ALIGNMENT_LIMIT:
            return match.group(0)
    return None


def function_body(lines, start):
    text = cpp_text("\n".join(lines[start:start + 400]))
    brace, semicolon = text.find("{"), text.find(";")
    if brace < 0 or 0 <= semicolon < brace:
        return None
    depth = 0
    for index in range(brace, len(text)):
        depth += {"{": 1, "}": -1}.get(text[index], 0)
        if depth == 0:
            return re.sub(r"\s+", " ", text[brace + 1:index]).replace(" ", "")
    return None


def output(check, repo):
    errors = 0
    rows = []
    actions = os.environ.get("GITHUB_ACTIONS") == "true"
    for rule, path, line, detail in sorted(check.findings, key=lambda f: (not f[1], f[1], f[2], f[0])):
        level, doc, message = RULES[rule]
        errors += level == "error"
        text = f"{message}" + (f": {detail}" if detail else "")
        if actions:
            escaped_path = path.replace("%", "%25").replace("\r", "%0D").replace("\n", "%0A").replace(":", "%3A").replace(",", "%2C")
            location = f" file={escaped_path},line={max(line, 1)}," if path else " "
            print(f"::{level}{location}title={rule}::" + text.replace("%", "%25").replace("\r", "%0D").replace("\n", "%0A"))
        else:
            print(f"{path or '(pull request)'}{f':{line}' if line else ''}: {level}: [{rule}] {text}")
        where = f"`{path}:{line}`" if line else f"`{path}`" if path else ""
        cell = text.replace("|", "\\|")
        rows.append(f"| {level} | [{rule}](https://github.com/{repo}/blob/main/{doc}) | {where} | {cell} |")
    summary = os.environ.get("GITHUB_STEP_SUMMARY")
    if summary:
        with open(summary, "a", encoding="utf-8") as out:
            if rows:
                out.write("| Level | Rule | Where | What |\n|---|---|---|---|\n" + "\n".join(rows) + "\n")
            else:
                out.write("All conventions checks passed.\n")
    print(f"{len(rows)} findings, {errors} errors; GitHub shows at most 10 error annotations, the job summary lists all"
          if rows else "All conventions checks passed.")
    return 1 if errors else 0


if __name__ == "__main__":
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    if hasattr(sys.stderr, "reconfigure"):
        sys.stderr.reconfigure(encoding="utf-8", errors="replace")
    parser = argparse.ArgumentParser(description="check a branch against the contributing rules; errors fail the check")
    parser.add_argument("--base", default="origin/main", help="branch the pull request targets (default: origin/main)")
    parser.add_argument("--head", default="HEAD")
    parser.add_argument("--title", help="pull request title to check")
    parser.add_argument("--body", help="file with the pull request description to check")
    parser.add_argument("--event", default=os.environ.get("GITHUB_EVENT_PATH"), help="GitHub event JSON (set in CI)")
    args = parser.parse_args()
    check = Check(args.base, args.head)
    check.code()
    check.build()
    check.shaders()
    check.repository()
    check.docs()
    check.debt_entries()
    check.workflows()
    check.commits()
    check.env_reference()
    pr = json.load(open(args.event, encoding="utf-8")).get("pull_request") if args.event else None
    if pr:
        check.pull_request(pr["title"], pr["body"])
    elif args.title is not None or args.body:
        check.pull_request(args.title or "", open(args.body, encoding="utf-8-sig").read() if args.body else "")
    raise SystemExit(output(check, os.environ.get("GITHUB_REPOSITORY", "boykopovar/AnyPS5")))
