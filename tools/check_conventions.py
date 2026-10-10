import argparse
import json
import os
import posixpath
import re
import subprocess
import sys
from collections import defaultdict
from pathlib import PurePosixPath

RULES = {
    "comment": ("error", "docs/dev/CONVENTIONS.md", "Comments are only for technical debt, so they need a change to docs/dev/TechnicalDebt.md in the same pull request; #endif and namespace ends are always allowed"),
    "silent-stub": ("error", "CONTRIBUTING.md#code", "Unimplemented exports call NotImplemented_nid_no_patch(__func__); silent stubs are listed in docs/dev/TechnicalDebt.md#silent-stubs, by their file or by name with a path in their library"),
    "duplicate-export": ("error", "CONTRIBUTING.md#code", "Already exported by another library: remove the copy, or use APS5_DUMMY_FUN if nothing is left"),
    "system-dependency": ("error", "CONTRIBUTING.md#code", "Third-party code is a submodule under 3rdparty/ built from source, not found on the system"),
    "title-specific": ("error", "CONTRIBUTING.md#code", "Implement the general behaviour, not what one title needs; title-specific code belongs in its sce_module"),
    "extension": ("error", "CONTRIBUTING.md#code", "Avoid non-standard extensions where standard C++ is enough"),
    "notes-file": ("error", "CONTRIBUTING.md#branches-and-pull-requests", "Notes, investigation and agent files go in the pull request, not in the repository"),
    "binary": ("error", "docs/dev/CONVENTIONS.md", "Only UTF-8 text files in the repository; images go in the gist comments"),
    "doc-link": ("error", "CONTRIBUTING.md#documentation", "Relative link to a file that does not exist"),
    "commit-subject": ("error", "docs/dev/CONVENTIONS.md", "Commit subject is not Conventional Commits"),
    "pr-title": ("error", "docs/dev/CONVENTIONS.md", "Pull request title is not Conventional Commits"),
    "pr-template": ("error", ".github/pull_request_template.md", "Fill in the pull request template"),
}

CPP = {".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx", ".inl", ".ipp"}
IMAGES = {".png", ".jpg", ".jpeg", ".gif", ".bmp", ".svg", ".webp", ".ico", ".tga", ".dds"}
NOTES = {".md", ".markdown", ".rst", ".log", ".patch", ".diff"}
AGENT_FILES = {"AGENTS.md", "CLAUDE.md", "GEMINI.md", ".cursorrules", "copilot-instructions.md"}

CONVENTIONAL = re.compile(r"^(feat|fix|docs|style|refactor|perf|test|build|ci|chore|revert)(\([^)]+\))?!?: \S")
STRINGS = re.compile(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'')
ALLOWED_COMMENT = re.compile(r"^\s*(#\s*endif\b|}\s*;?\s*//\s*(end\s+)?namespace\b)")
EXPORT = re.compile(r"\bAPS5_VABI\s+(\w+)\s*\(")
STUB_BODY = re.compile(r"^(?:\(void\)\s*\w+\s*;|static_cast<void>\(\s*\w+\s*\)\s*;)*"
                       r"(?:return\s*\(?\s*(?:-?(?:0x[0-9a-fA-F]+|\d+)[uUlL]*|nullptr|true|false|\w*_OK)?\s*\)?\s*;)?$")
SYSTEM_DEPENDENCY = re.compile(r"\b(find_package\s*\((?!\s*(Python3|Threads|Git)\b)|pkg_check_modules|pkg_search_module|find_library)\b")
TITLE = re.compile(r"\b(PPSA|CUSA)\d{5}\b")
EXTENSION = re.compile(r"__attribute__|__declspec|#\s*pragma\s+(?!once\b)")
LINK = re.compile(r"\]\(([^)\s]+)")
SECTION = re.compile(r"^###\s*(.+?)\s*$", re.M)


def git(*args, text=True):
    output = subprocess.run(["git", *args], capture_output=True, check=True).stdout
    return output.decode("utf-8", errors="replace") if text else output


class Check:
    def __init__(self, base, head):
        self.head = head
        self.base = git("merge-base", base, head).strip()
        self.findings = []
        self.added = defaultdict(list)
        self.status = {}
        self.numstat = {}
        self.tree = set(git("ls-tree", "-r", "--name-only", head).splitlines())
        self.dirs = {str(parent) for path in self.tree for parent in PurePosixPath(path).parents}
        for line in git("diff", "--name-status", "--no-renames", self.base, head).splitlines():
            status, path = line.split("\t", 1)
            self.status[path] = status
        for line in git("diff", "--numstat", "--no-renames", self.base, head).splitlines():
            added, deleted, path = line.split("\t", 2)
            self.numstat[path] = (added, deleted)
        path = None
        for line in git("diff", "-U0", "--no-color", "--no-renames", "--no-prefix", self.base, head).splitlines():
            if line.startswith("+++ "):
                path = None if line == "+++ /dev/null" else line[4:]
            elif line.startswith("@@"):
                number = int(re.search(r"\+(\d+)", line).group(1))
            elif line.startswith("+") and path:
                self.added[path].append((number, line[1:]))
                number += 1

    def report(self, rule, path, line, detail=""):
        self.findings.append((rule, path, line, detail))

    def files(self, predicate):
        return [(path, lines) for path, lines in self.added.items() if not path.startswith("3rdparty/") and predicate(PurePosixPath(path))]

    def show(self, path):
        return git("show", f"{self.head}:{path}")

    def code(self):
        debt = [(text, links("docs/dev/TechnicalDebt.md", text)) for text in self.show("docs/dev/TechnicalDebt.md").splitlines()] if "docs/dev/TechnicalDebt.md" in self.tree else []
        exports = {}
        for path, lines in self.files(lambda p: p.suffix in CPP):
            core = path.startswith("core/") and "tests" not in PurePosixPath(path).parts
            source = None
            for number, text in lines:
                bare = STRINGS.sub('""', text)
                if ("//" in bare or "/*" in bare) and not ALLOWED_COMMENT.match(bare):
                    self.report("comment", path, number)
                if EXTENSION.search(bare):
                    self.report("extension", path, number, EXTENSION.search(bare).group(0))
                if core and TITLE.search(text):
                    self.report("title-specific", path, number, TITLE.search(text).group(0))
                match = EXPORT.search(bare)
                if match and path.startswith("core/libs/prx/") and not match.group(1).endswith("_nid_no_patch"):
                    source = source or self.show(path).splitlines()
                    body = function_body(source, number - 1)
                    if body is None:
                        continue
                    exports[match.group(1)] = (path, number)
                    if match.group(1).startswith("sce") and STUB_BODY.match(body) and not listed(debt, match.group(1), path):
                        self.report("silent-stub", path, number, match.group(1))
        self.duplicates(exports)
        for path, lines in self.files(lambda p: p.suffix in (".py", ".cmake") or p.name == "CMakeLists.txt"):
            for number, text in lines:
                if "#" in STRINGS.sub('""', text) and not text.startswith("#!"):
                    self.report("comment", path, number)
        if "docs/dev/TechnicalDebt.md" in self.status:
            self.findings = [finding for finding in self.findings if finding[0] != "comment"]

    def duplicates(self, exports):
        if not exports:
            return
        pattern = r"(^|[^[:alnum:]_])APS5_VABI[[:space:]]+(" + "|".join(exports) + r")[[:space:]]*\("
        result = subprocess.run(["git", "grep", "-nE", pattern, self.head, "--", "core/libs/prx"], capture_output=True, encoding="utf-8", errors="replace")
        for line in result.stdout.splitlines():
            _, path, _, text = line.split(":", 3)
            name = EXPORT.search(text).group(1)
            own, number = exports[name]
            if library(path) != library(own) and not text.rstrip().endswith(";"):
                self.report("duplicate-export", own, number, f"{name} is also in {library(path)}")

    def build(self):
        for path, lines in self.files(lambda p: p.name == "CMakeLists.txt" or p.suffix == ".cmake"):
            for number, text in lines:
                if SYSTEM_DEPENDENCY.search(text.split("#", 1)[0]):
                    self.report("system-dependency", path, number, text.strip())
        for number, text in self.added.get(".gitmodules", []):
            match = re.match(r"\s*path\s*=\s*(\S+)", text)
            if match and not match.group(1).startswith("3rdparty/"):
                self.report("system-dependency", ".gitmodules", number, match.group(1))

    def repository(self):
        for path, (added, deleted) in self.numstat.items():
            if path.startswith("3rdparty/"):
                continue
            suffix = PurePosixPath(path).suffix.lower()
            if self.status.get(path) != "D" and (added == "-" or suffix in IMAGES or not utf8(git("show", f"{self.head}:{path}", text=False))):
                self.report("binary", path, 0)
            if self.status.get(path) == "A" and allowed_notes(path) is False:
                self.report("notes-file", path, 0)

    def docs(self):
        for path, lines in self.files(lambda p: p.suffix == ".md"):
            for number, text in lines:
                for target, resolved in links(path, text):
                    if resolved not in self.tree and resolved not in self.dirs:
                        self.report("doc-link", path, number, target)

    def commits(self):
        for line in git("log", "--no-merges", "--format=%h %s", f"{self.base}..{self.head}").splitlines():
            sha, subject = line.split(" ", 1)
            if not CONVENTIONAL.match(subject):
                self.report("commit-subject", "", 0, f"{sha} {subject}")

    def pull_request(self, title, body):
        if not CONVENTIONAL.match(title):
            self.report("pr-title", "", 0, title)
        body = re.sub(r"<!--.*?-->", "", body or "", flags=re.S)
        sections = dict(zip(SECTION.findall(body), SECTION.split(body)[2::2]))
        for name in ("What", "Tested", "Checklist"):
            if not sections.get(name, "").strip():
                self.report("pr-template", "", 0, f"section '{name}' is missing or empty")
        checklist = sections.get("Checklist", "")
        for item in re.findall(r"^\s*-\s*\[ \]\s*(.+)$", checklist, re.M):
            self.report("pr-template", "", 0, f"unchecked: {item.strip()}")
        if checklist and not re.search(r"AI-assisted:\s*(yes|no)\b", checklist, re.I):
            self.report("pr-template", "", 0, "answer AI-assisted with yes or no")
        if checklist and not re.search(r"Depends on:\s*(#\d+|none)\b", checklist, re.I):
            self.report("pr-template", "", 0, "fill Depends on with #N or none")


def utf8(data):
    try:
        data.decode("utf-8")
    except UnicodeDecodeError:
        return False
    return True


def links(path, text):
    result = []
    for target in LINK.findall(text):
        if re.match(r"[a-z]+:|#", target):
            continue
        target = target.split("#", 1)[0].split("?", 1)[0]
        result.append((target, posixpath.normpath(target.lstrip("/") if target.startswith("/") else str(PurePosixPath(path).parent / target))))
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


def allowed_notes(path):
    p = PurePosixPath(path)
    if p.name in AGENT_FILES:
        return False
    if p.suffix.lower() not in NOTES:
        return None
    return path in ("README.md", "CONTRIBUTING.md") or p.parts[0] == ".github" or p.parts[:2] in (("docs", "dev"), ("docs", "user"))


def function_body(lines, start):
    text = "\n".join(lines[start:start + 400])
    text = re.sub(r"//[^\r\n]*|/\*.*?\*/", "", STRINGS.sub('""', text), flags=re.S)
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
            location = f" file={path},line={max(line, 1)}," if path else " "
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
    check.repository()
    check.docs()
    check.commits()
    pr = json.load(open(args.event, encoding="utf-8")).get("pull_request") if args.event else None
    if pr:
        check.pull_request(pr["title"], pr["body"])
    elif args.title is not None or args.body:
        check.pull_request(args.title or "", open(args.body, encoding="utf-8").read() if args.body else "")
    raise SystemExit(output(check, os.environ.get("GITHUB_REPOSITORY", "boykopovar/AnyPS5")))
