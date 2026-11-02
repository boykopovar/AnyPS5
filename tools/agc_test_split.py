import argparse
import re
import subprocess
import sys
import tempfile
from pathlib import Path

LISTS = "core/libs/prx/libSceAgcDriver/CMakeLists.txt"
FRAGMENTS = "core/libs/prx/libSceAgcDriver/tests/cmake"
EXECUTION = "core/libs/prx/libSceAgcDriver/tests/execution"
INCLUDE = [
    "file(GLOB agcTestFragments CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake/*.cmake)",
    "list(SORT agcTestFragments)",
    "foreach(agcTestFragment IN LISTS agcTestFragments)",
    "    include(${agcTestFragment})",
    "endforeach()",
]

OPEN = {"if": "endif", "foreach": "endforeach", "while": "endwhile", "function": "endfunction", "macro": "endmacro", "block": "endblock"}
BRANCH = {"elseif", "else"}
CALL = re.compile(r"[ \t]*([A-Za-z_][A-Za-z0-9_]*)[ \t]*\(")
UNIT = re.compile(r"[a-z][a-z0-9_]*_tests")
MENTION = re.compile(r"(?<![A-Za-z0-9_{])[a-z][a-z0-9_]*_tests(?![A-Za-z0-9_])")
VARIABLE = re.compile(r"\$\{([A-Za-z0-9_]+)\}")
IDENT = re.compile(r"[A-Za-z_][A-Za-z0-9_]*")
SPECIAL = re.compile(r'if\(agcTestName STREQUAL "([A-Za-z0-9_]+)"\)')
DEFINERS = {"add_executable", "add_test_executable", "add_library", "add_custom_target"}
SUBJECT_FIRST = {"target_link_libraries", "target_include_directories", "target_compile_definitions", "target_compile_options",
                 "target_sources", "target_link_options", "target_compile_features", "target_precompile_headers",
                 "configure_windows_unwind", "add_dependencies"}
CONTROL = {"if", "elseif", "else", "endif", "foreach", "endforeach", "while", "endwhile", "function", "endfunction", "macro",
           "endmacro", "block", "endblock", "return", "break", "continue", "set", "unset", "list", "string", "file", "message",
           "math", "option", "include", "get_filename_component", "add_custom_command", "add_subdirectory", "cmake_path",
           "set_source_files_properties", "separate_arguments"}
SETTERS = {"set", "unset", "option", "list", "string", "math", "file", "get_filename_component", "get_target_property",
           "get_test_property", "get_directory_property", "get_property", "cmake_path", "separate_arguments", "foreach"}
LIST_READERS = {"APPEND", "PREPEND", "INSERT", "REMOVE_ITEM", "REMOVE_AT", "REMOVE_DUPLICATES", "FILTER", "SORT", "REVERSE",
                "TRANSFORM", "POP_BACK", "POP_FRONT", "GET", "LENGTH", "FIND", "JOIN"}
HELPERS = {
    "agc_add_default_test": lambda target: (target, [target[:-6]]),
    "agc_add_draw_pipeline_test": lambda stem: (stem + "_tests", [stem + "_draw_pipeline"]),
    "agc_add_spirv_mode_tests": lambda target: (target, [target[:-6], target[:-6] + "_full", target[:-6] + "_none"]),
}
TEST_KEYWORDS = {"CONFIGURATIONS", "WORKING_DIRECTORY", "COMMAND_EXPAND_LISTS"}


class SplitError(Exception):
    pass


class Command:
    def __init__(self, name, arguments, first, last):
        self.name = name
        self.arguments = arguments
        self.first = first
        self.last = last


class Block:
    def __init__(self, header):
        self.header = header
        self.branches = [(header, [])]
        self.end = None
        self.first = header.first

    @property
    def last(self):
        return self.end.last


def arguments_of(text, position, line):
    arguments, depth, token = [], 0, None
    while True:
        if position >= len(text):
            raise SplitError(f"line {line + 1}: unterminated command")
        character = text[position]
        if character == "\n":
            line += 1
        if character == '"':
            end = position + 1
            while text[end] != '"':
                if text[end] == "\\":
                    end += 1
                if text[end] == "\n":
                    line += 1
                end += 1
            token = (token or "") + text[position + 1:end]
            position = end + 1
            continue
        bracket = re.match(r"\[(=*)\[", text[position:position + 32])
        if bracket and token is None:
            close = text.index("]" + bracket[1] + "]", position)
            value = text[position + len(bracket[0]):close]
            line += value.count("\n")
            arguments.append(value)
            position = close + len(bracket[0])
            continue
        if character == "#":
            comment = re.match(r"#\[(=*)\[", text[position:position + 32])
            close = text.index("]" + comment[1] + "]", position) + len(comment[0]) if comment else text.find("\n", position)
            line += text[position:close].count("\n")
            position = close
            continue
        if character == "\\" and token is not None:
            token += text[position:position + 2]
            position += 2
            continue
        if character in " \t\r\n()":
            if token is not None:
                arguments.append(token)
                token = None
            if character == "(":
                depth += 1
                arguments.append("(")
            elif character == ")":
                if depth == 0:
                    return arguments, position + 1, line
                depth -= 1
                arguments.append(")")
            position += 1
            continue
        token = (token or "") + character
        position += 1


def commands(text):
    lines = text.split("\n")
    offsets = [0]
    for item in lines:
        offsets.append(offsets[-1] + len(item) + 1)
    found, index = [], 0
    while index < len(lines):
        stripped = lines[index].strip()
        if not stripped or stripped.startswith("#"):
            index += 1
            continue
        call = CALL.match(lines[index])
        if not call:
            raise SplitError(f"line {index + 1}: not a command: {lines[index]}")
        arguments, end, last = arguments_of(text, offsets[index] + call.end(), index)
        rest = text[end:offsets[last + 1] - 1] if last + 1 < len(offsets) else text[end:]
        if rest.strip() and not rest.strip().startswith("#"):
            raise SplitError(f"line {last + 1}: more than one command on a line")
        found.append(Command(call[1].lower(), arguments, index, last))
        index = last + 1
    return found


def tree(text):
    root, stack = [], []
    for command in commands(text):
        if command.name in OPEN:
            block = Block(command)
            (stack[-1].branches[-1][1] if stack else root).append(block)
            stack.append(block)
        elif command.name in BRANCH:
            if not stack or stack[-1].header.name != "if":
                raise SplitError(f"line {command.first + 1}: {command.name} outside if")
            stack[-1].branches.append((command, []))
        elif command.name in OPEN.values():
            if not stack or OPEN[stack[-1].header.name] != command.name:
                raise SplitError(f"line {command.first + 1}: unmatched {command.name}")
            stack.pop().end = command
        else:
            (stack[-1].branches[-1][1] if stack else root).append(command)
    if stack:
        raise SplitError(f"line {stack[-1].first + 1}: unclosed {stack[-1].header.name}")
    return root


def flatten(node):
    if isinstance(node, Command):
        yield node
        return
    for branch, children in node.branches:
        yield branch
        for child in children:
            yield from flatten(child)
    yield node.end


class Atom:
    def __init__(self, node, conditions):
        self.node = node
        self.conditions = conditions

    def flat(self):
        return flatten(self.node)


def atoms(nodes, conditions=()):
    for node in nodes:
        if isinstance(node, Block) and node.header.name == "if" and len(node.branches) == 1:
            yield from atoms(node.branches[0][1], conditions + (node,))
        else:
            yield Atom(node, conditions)


def enclosing_loop(lines, start):
    depth = 0
    for index in range(start, len(lines)):
        stripped = lines[index].strip()
        if re.match(r"foreach\s*\(", stripped):
            depth += 1
        elif re.match(r"endforeach\s*\(", stripped):
            depth -= 1
            if depth == 0:
                return start, index, len(lines[start]) - len(lines[start].lstrip())
    raise SplitError(f"line {start + 1}: unterminated loop")


def find_loop(lines, predicate, what):
    starts = [index for index, line in enumerate(lines) if predicate(index, line.strip())]
    if len(starts) > 1:
        raise SplitError(f"more than one {what}")
    return enclosing_loop(lines, starts[0]) if starts else None


def dedent(lines, amount):
    return [line[amount:] if line.startswith(" " * amount) else line for line in lines]


def loop_list(header):
    return header[header.index("ITEMS") + len("ITEMS"):header.rindex(")")].split()


def default_tests(lines):
    found = find_loop(lines, lambda index, line: line.startswith("foreach(agcTest IN ITEMS ") and "${agcExecutionTests}" in line,
                      "default-test loop")
    if found is None:
        raise SplitError("the default-test loop over ${agcExecutionTests} is missing")
    start, end, indent = found
    body = lines[start + 1:end]
    skip = []
    if body and body[0].strip().startswith("if(agcTest STREQUAL"):
        close = next(index for index, line in enumerate(body) if line.strip() == "endif()")
        skip, body = body[:close + 1], body[close + 1:]
    kept, tweaks, index = [], [], 0
    while index < len(body):
        special = SPECIAL.fullmatch(body[index].strip())
        if not special:
            kept.append(body[index])
            index += 1
            continue
        depth = len(body[index]) - len(body[index].lstrip())
        close = next(position for position in range(index, len(body))
                     if body[position].strip() == "endif()" and len(body[position]) - len(body[position].lstrip()) == depth)
        inner = body[index + 1:close]
        if not inner or not inner[0].strip().startswith("set_tests_properties(${agcTestName} "):
            raise SplitError(f"line {start + index + 2}: a default-test case does more than set properties")
        tweaks += [" " * indent + line.strip().replace("${agcTestName}", special[1]) if position == 0 else line[8:]
                   for position, line in enumerate(inner)]
        index = close + 1
    pad = " " * indent
    lines[start:end + 1] = [f"{pad}agc_add_default_test({name})" for name in loop_list(lines[start])
                            if name != "${agcExecutionTests}"] + tweaks
    function = ["function(agc_add_default_test agcTest)"] + dedent(kept, indent) + ["endfunction()"]
    execution = ["if(BUILD_TESTING)", "    foreach(agcTest IN LISTS agcExecutionTests)"] + \
        [" " * 8 + line[indent + 4:] for line in skip] + ["        agc_add_default_test(${agcTest})", "    endforeach()", "endif()"]
    return function, execution


def draw_pipeline_tests(lines):
    found = find_loop(lines, lambda index, line: line == "foreach(agcTestName IN LISTS agcDrawPipelineTests)", "draw pipeline loop")
    if found is None:
        return None
    start, end, indent = found
    listing = lines[start - 1].strip()
    if not listing.startswith("set(agcDrawPipelineTests "):
        raise SplitError("agcDrawPipelineTests is not set right before its loop")
    names = listing[len("set(agcDrawPipelineTests "):-1].split()
    body = lines[start + 1:end]
    lines[start - 1:end + 1] = [f"{' ' * indent}agc_add_draw_pipeline_test({name})" for name in names]
    return ["function(agc_add_draw_pipeline_test agcTestName)"] + dedent(body, indent) + ["endfunction()"]


def spirv_mode_tests(lines):
    found = find_loop(lines, lambda index, line: line.startswith("foreach(agcTest IN ITEMS ") and index + 1 < len(lines)
                      and lines[index + 1].strip() == "if(NOT TARGET ${agcTest})", "SPIR-V mode loop")
    if found is None:
        return None
    start, end, indent = found
    body = lines[start + 1:end]
    if body[1].strip() != "continue()":
        raise SplitError("the SPIR-V mode loop no longer starts with its target check")
    body[1] = body[1].replace("continue()", "return()")
    lines[start:end + 1] = [f"{' ' * indent}agc_add_spirv_mode_tests({name})" for name in loop_list(lines[start])]
    return ["function(agc_add_spirv_mode_tests agcTest)"] + dedent(body, indent) + ["endfunction()"]


def refactor(text):
    if "function(agc_add_default_test " in text:
        return text
    lines = text.split("\n")
    function, execution = default_tests(lines)
    functions = [function]
    for convert in (draw_pipeline_tests, spirv_mode_tests):
        function = convert(lines)
        if function:
            functions.append(function)
    glob = [index for index, line in enumerate(lines) if line.startswith("foreach(agcExecutionTestSource IN LISTS agcExecutionTestSources)")]
    if len(glob) != 1:
        raise SplitError("the execution-test loop is missing")
    close = next(index for index in range(glob[0], len(lines)) if lines[index] == "endforeach()")
    added = [""]
    for function in functions:
        added += function + [""]
    lines[close + 1:close + 1] = added + execution + [""]
    return "\n".join(lines)


class Facts:
    def __init__(self):
        self.targets = set()
        self.tests = {}
        self.dynamic_tests = []
        self.subject_targets = set()
        self.subject_tests = set()
        self.test_patterns = set()
        self.dynamic = set()
        self.defines = set()
        self.accumulates = set()
        self.uses = set()
        self.loop = set()
        self.mentions = set()
        self.calls = set()
        self.functions = set()
        self.reads_all_tests = False
        self.creates_defaults = False
        self.dynamic_targets = False


def until(arguments, keywords):
    for index, argument in enumerate(arguments):
        if argument in keywords:
            return arguments[:index]
    return arguments


def literal(name):
    return "${" not in name and "$<" not in name


def subject(facts, names, kind):
    for name in names:
        if literal(name):
            (facts.subject_targets if kind == "target" else facts.subject_tests).add(name)
        else:
            facts.dynamic.update(VARIABLE.findall(name))
            if kind == "test":
                facts.test_patterns.add(name)


def loop_items(atom):
    items = {}
    for command in atom.flat():
        arguments = command.arguments
        if command.name != "foreach" or not arguments:
            continue
        rest = arguments[1:]
        if rest[:1] == ["IN"]:
            if rest[1:2] != ["ITEMS"]:
                continue
            rest = rest[2:]
        elif rest[:1] == ["RANGE"]:
            bounds = rest[1:]
            if not all(bound.isdigit() for bound in bounds) or not 1 <= len(bounds) <= 3:
                continue
            numbers = [int(bound) for bound in bounds]
            start, stop = (0, numbers[0]) if len(numbers) == 1 else (numbers[0], numbers[1])
            rest = [str(value) for value in range(start, stop + 1, numbers[2] if len(numbers) == 3 else 1)]
        if all(literal(item) for item in rest):
            items[arguments[0]] = rest
    return items


def expand(name, items):
    names = [name]
    for variable in set(VARIABLE.findall(name)):
        if variable not in items:
            return []
        names = [candidate.replace("${" + variable + "}", value) for candidate in names for value in items[variable]]
    return names


def facts_of(atom):
    facts = Facts()
    if isinstance(atom.node, Block) and atom.node.header.name in ("function", "macro"):
        facts.functions.add(atom.node.header.arguments[0].lower())
        return facts
    for command in atom.flat():
        arguments, name = command.arguments, command.name
        facts.calls.add(name)
        for argument in arguments:
            facts.uses.update(VARIABLE.findall(argument))
            if "$<" not in argument:
                facts.mentions.update(MENTION.findall(argument))
        if name in ("if", "elseif", "while"):
            facts.uses.update(argument for argument in arguments if IDENT.fullmatch(argument))
        if name == "foreach" and arguments:
            facts.loop.add(arguments[0])
            if "LISTS" in arguments:
                facts.uses.update(argument for argument in arguments[arguments.index("LISTS") + 1:]
                                  if IDENT.fullmatch(argument) and argument != "ITEMS")
        for index, argument in enumerate(arguments[1:-1], 1):
            if argument == "IN_LIST":
                facts.uses.add(arguments[index + 1])
        if name in ("list", "string") and len(arguments) > 1 and arguments[0] in LIST_READERS:
            facts.uses.add(arguments[1])
            facts.accumulates.add(arguments[1])
        if name in SETTERS:
            for index, argument in enumerate(arguments):
                if IDENT.fullmatch(argument) and not (name == "foreach" and index):
                    facts.defines.add(argument)
                if name in ("set", "unset", "option", "foreach"):
                    break
        if name in HELPERS and arguments:
            if literal(arguments[0]):
                executable, tests = HELPERS[name](arguments[0])
                facts.tests.update({test: executable for test in tests})
                if name != "agc_add_draw_pipeline_test":
                    facts.subject_targets.add(arguments[0])
            else:
                facts.dynamic.update(VARIABLE.findall(arguments[0]))
                facts.dynamic_tests.append((arguments[0], None))
                facts.creates_defaults = True
        if name in DEFINERS and arguments:
            if literal(arguments[0]):
                facts.targets.add(arguments[0])
            else:
                facts.dynamic.update(VARIABLE.findall(arguments[0]))
                facts.dynamic_targets = True
        elif name == "add_test" and arguments:
            if arguments[0] == "NAME":
                test = arguments[1]
                rest = until(arguments[2:], TEST_KEYWORDS)
                executable = rest[rest.index("COMMAND") + 1] if "COMMAND" in rest[:-1] else None
            else:
                test, executable = arguments[0], arguments[1] if len(arguments) > 1 else None
            if literal(test):
                facts.tests[test] = executable
            else:
                facts.dynamic_tests.append((test, executable))
        elif name in SUBJECT_FIRST and arguments:
            subject(facts, arguments[:1], "target")
        elif name == "add_asan_test_options" and arguments:
            subject(facts, arguments[:1], "test")
        elif name == "set_target_properties":
            subject(facts, until(arguments, {"PROPERTIES"}), "target")
        elif name == "set_tests_properties":
            subject(facts, until(arguments, {"PROPERTIES", "DIRECTORY"}), "test")
        elif name == "set_property" and len(arguments) > 1 and arguments[0] in ("TARGET", "TEST"):
            subject(facts, until(arguments[1:], {"PROPERTY", "APPEND", "APPEND_STRING", "DIRECTORY"}), arguments[0].lower())
        elif name in ("get_target_property", "get_test_property") and len(arguments) > 1:
            subject(facts, arguments[1:2], "target" if name == "get_target_property" else "test")
        elif name == "get_directory_property" and "TESTS" in arguments:
            facts.reads_all_tests = True
        elif name not in CONTROL and name not in HELPERS and arguments and UNIT.fullmatch(arguments[0]):
            subject(facts, arguments[:1], "target")
    items = loop_items(atom)
    for test, executable in facts.dynamic_tests:
        for name in expand(test, items):
            facts.tests.setdefault(name, executable)
    facts.test_patterns = {prefix for prefix in (re.split(r"\$[{<]", name)[0] for name in
                                                 facts.test_patterns - {test for test, _executable in facts.dynamic_tests}) if prefix}
    return facts


def union_find(size):
    parent = list(range(size))

    def find(index):
        while parent[index] != index:
            parent[index] = parent[parent[index]]
            index = parent[index]
        return index

    def join(first, second):
        parent[find(first)] = find(second)

    return find, join


def reads(facts):
    return (facts.uses - (facts.defines - facts.accumulates)) | facts.accumulates


class Plan:
    def __init__(self, text, execution=()):
        self.lines = text.split("\n")
        self.roots = tree(text)
        self.atoms = list(atoms(self.roots))
        self.facts = [facts_of(atom) for atom in self.atoms]
        self.known = {name for facts in self.facts for name in facts.targets | facts.subject_targets | set(facts.tests.values())
                      if name and UNIT.fullmatch(name)} | set(execution)
        self.external = self.known - {name for facts in self.facts for name in facts.targets}
        self.test_units = {test: self.unit_of_test(test, executable) for facts in self.facts for test, executable in facts.tests.items()}
        self.units = self.assign()
        self.include = self.place()

    def unit_of_test_name(self, test):
        stems = [unit for unit in self.known if test == unit[:-6] or test.startswith(unit[:-6] + "_")]
        return max(stems, key=len) if stems else None

    def unit_of_test(self, test, executable):
        return executable if executable and literal(executable) and UNIT.fullmatch(executable) else self.unit_of_test_name(test)

    def candidates(self, index, test_owner):
        facts = self.facts[index]
        owners, unknown = set(), False
        for target in facts.targets | facts.subject_targets:
            if UNIT.fullmatch(target):
                owners.add(target)
            else:
                unknown = True
        for test, executable in facts.tests.items():
            owner = self.unit_of_test(test, executable)
            if owner:
                owners.add(owner)
        for test in facts.subject_tests - set(facts.tests):
            if test in test_owner:
                owners.add(test_owner[test])
            elif test + "_tests" in self.external:
                owners.add(test + "_tests")
            else:
                unknown = True
        for prefix in facts.test_patterns:
            matched = {unit for test, unit in self.test_units.items() if test.startswith(prefix)}
            if len(matched) == 1 and None not in matched:
                owners |= matched
            elif matched:
                unknown = True
        if facts.dynamic - facts.defines or facts.reads_all_tests or facts.creates_defaults or facts.dynamic_targets:
            unknown = True
        if facts.dynamic or facts.dynamic_tests:
            for _test, executable in facts.dynamic_tests:
                if executable and literal(executable) and UNIT.fullmatch(executable):
                    owners.add(executable)
            if facts.dynamic and len(facts.mentions | owners) > 1:
                unknown = True
            owners |= facts.mentions
        return owners, unknown

    def write_groups(self):
        defining = {}
        for index, facts in enumerate(self.facts):
            for name in facts.defines:
                defining.setdefault(name, []).append(index)
        mutable = {name for name, indices in defining.items() if len(indices) > 1}
        mutable |= set().union(*(facts.accumulates for facts in self.facts))
        find, join = union_find(len(self.atoms))
        for index, facts in enumerate(self.facts):
            for name in reads(facts) & mutable & facts.defines:
                for earlier in reversed([other for other in defining[name] if other < index]):
                    join(earlier, index)
                    if name in self.facts[earlier].defines - self.facts[earlier].accumulates:
                        break
        return [find(index) for index in range(len(self.atoms))]

    def variable_components(self, free):
        defining = {}
        find, join = union_find(len(self.atoms))
        for index, facts in enumerate(self.facts):
            for name in reads(facts):
                for earlier in reversed(defining.get(name, [])):
                    if free[earlier] or free[index]:
                        join(earlier, index)
                    if name in self.facts[earlier].defines - self.facts[earlier].accumulates:
                        break
            for name in facts.defines - facts.loop:
                defining.setdefault(name, []).append(index)
        components = {}
        for index in range(len(self.atoms)):
            components.setdefault(find(index), []).append(index)
        return [indices for indices in components.values() if any(free[index] for index in indices)]

    def assign(self):
        group = self.write_groups()
        members = {}
        for index in range(len(self.atoms)):
            members.setdefault(group[index], []).append(index)
        test_owner, pinned = {}, set()
        while True:
            units, unknowns = [], []
            for index in range(len(self.atoms)):
                owners, unknown = self.candidates(index, test_owner)
                units.append(owners)
                unknowns.append(unknown or group[index] in pinned)
            fresh_pinned = set(pinned)
            for root, indices in members.items():
                if len(indices) > 1 and any(unknowns[index] for index in indices) and any(units[index] for index in indices):
                    fresh_pinned.add(root)
            result = []
            for index in range(len(self.atoms)):
                if unknowns[index] or group[index] in fresh_pinned:
                    result.append(None)
                else:
                    result.append(set().union(*(units[other] for other in members[group[index]])) or None)
            first = min((index for index in range(len(self.atoms)) if result[index]), default=len(self.atoms))
            free = [not result[index] and not unknowns[index] and not units[index] and index > first
                    and group[index] not in fresh_pinned for index in range(len(self.atoms))]
            for indices in self.variable_components(free):
                if any(not free[index] and not result[index] for index in indices):
                    continue
                owners = {frozenset(result[index]) for index in indices if result[index]}
                if len(owners) == 1:
                    for index in indices:
                        if free[index]:
                            result[index] = set(next(iter(owners)))
            fresh = {}
            for index, owners in enumerate(result):
                if owners and len(owners) == 1:
                    for test in self.facts[index].tests:
                        fresh[test] = next(iter(owners))
            if fresh == test_owner and fresh_pinned == pinned:
                break
            test_owner, pinned = fresh, fresh_pinned
        names = sorted({name for owners in result if owners for name in owners})
        position = {name: index for index, name in enumerate(names)}
        find, join = union_find(len(names))
        sizes = {}
        for index, owners in enumerate(result):
            if not owners:
                continue
            ordered = sorted(owners)
            for name in ordered:
                join(position[name], position[ordered[0]])
                sizes[name] = sizes.get(name, 0) + self.atoms[index].node.last - self.atoms[index].node.first + 1
        largest = {}
        for name in names:
            root = find(position[name])
            if root not in largest or (sizes[name], name) > (sizes[largest[root]], largest[root]):
                largest[root] = name
        return [largest[find(position[next(iter(owners))])] if owners else None for owners in result]

    def place(self):
        moved = [index for index, unit in enumerate(self.units) if unit]
        if not moved:
            raise SplitError(f"{LISTS} registers no tests to move")

        def union(field):
            return set().union(*(getattr(self.facts[index], field) for index in moved))

        defined, used, targets, calls = union("defines"), union("uses"), union("targets"), union("calls")
        tests = set().union(*(set(self.facts[index].tests) for index in moved))
        everywhere = set().union(*(facts.targets for facts in self.facts))
        external = {name for name in union("subject_targets") if UNIT.fullmatch(name) and name not in everywhere}
        needed, depends = [], []
        for index, facts in enumerate(self.facts):
            if self.units[index]:
                continue
            if ((facts.defines - facts.loop) & used or (facts.dynamic_targets and external) or facts.creates_defaults
                    or facts.functions & calls):
                needed.append(index)
            if ((facts.uses - facts.defines) & defined or facts.subject_targets & targets or facts.subject_tests & tests
                    or facts.mentions & targets or facts.reads_all_tests
                    or any(test.startswith(prefix) for prefix in facts.test_patterns for test in tests)):
                depends.append(index)
        top = [next(position for position, node in enumerate(self.roots) if node.first <= atom.node.first <= node.last)
               for atom in self.atoms]
        low = top[max(needed)] + 1 if needed else 0
        high = top[min(depends)] if depends else len(self.roots)
        if low > high:
            raise SplitError(f"no single place to include the fragments: line {self.atoms[max(needed)].node.first + 1} must come "
                             f"before them and line {self.atoms[min(depends)].node.first + 1} after them")
        return max(low, min(high, max(top[index] for index in moved) + 1))


def leading(lines, first):
    while first > 0 and lines[first - 1].lstrip().startswith("#"):
        first -= 1
    return first


def opener(line):
    call = CALL.match(line)
    return bool(call) and (call[1].lower() in OPEN or call[1].lower() in BRANCH)


def closer(line):
    call = CALL.match(line)
    return bool(call) and (call[1].lower() in OPEN.values() or call[1].lower() in BRANCH)


def wrappers(nodes):
    for node in nodes:
        if isinstance(node, Block):
            for _branch, children in node.branches:
                yield from wrappers(children)
            yield node


def fragment_text(lines, members):
    out, stack = [], []
    for atom in members:
        conditions = list(atom.conditions)
        common = 0
        while common < min(len(stack), len(conditions)) and stack[common] is conditions[common]:
            common += 1
        while len(stack) > common:
            block = stack.pop()
            out.extend(lines[block.end.first:block.end.last + 1])
        first = leading(lines, atom.node.first)
        separated = first > 0 and not lines[first - 1].strip()
        for block in conditions[common:]:
            if out and not stack and out[-1].strip():
                out.append("")
            out.extend(lines[block.header.first:block.header.last + 1])
            stack.append(block)
        if out and separated and out[-1].strip() and not opener(out[-1]):
            out.append("")
        out.extend(line for line in lines[first:atom.node.last + 1] if not line.lstrip().startswith("#"))
    while stack:
        block = stack.pop()
        out.extend(lines[block.end.first:block.end.last + 1])
    return "\n".join(out) + "\n"


def split_text(text, execution=()):
    plan = Plan(text, execution)
    lines = plan.lines
    units = {}
    for atom, unit in zip(plan.atoms, plan.units):
        if unit:
            units.setdefault(unit, []).append(atom)
    deleted = set()
    for members in units.values():
        for atom in members:
            deleted.update(range(leading(lines, atom.node.first), atom.node.last + 1))
    for block in wrappers(plan.roots):
        if block.header.name != "if" or len(block.branches) != 1:
            continue
        body = range(block.header.last + 1, block.end.first)
        if all(index in deleted or not lines[index].strip() or lines[index].lstrip().startswith("#") for index in body):
            deleted.update(range(leading(lines, block.header.first), block.end.last + 1))
    at = leading(lines, plan.roots[plan.include].first) if plan.include < len(plan.roots) else len(lines)
    kept = []
    for index, line in enumerate(lines):
        if index == at:
            kept.extend([""] + INCLUDE + [""])
        if index not in deleted:
            kept.append(line)
    if at >= len(lines):
        kept.extend([""] + INCLUDE)
    residual = []
    for line in kept:
        if not line.strip() and (not residual or not residual[-1].strip() or opener(residual[-1])):
            continue
        if closer(line) and residual and not residual[-1].strip():
            residual.pop()
        residual.append(line)
    while residual and not residual[-1].strip():
        residual.pop()
    return "\n".join(residual) + "\n", {name: fragment_text(lines, members) for name, members in units.items()}


def migrated(text, execution=()):
    return split_text(refactor(text), execution)


def execution_target(stem):
    return "agc_driver_" + re.sub(r"([a-z0-9])([A-Z])", r"\1_\2", stem).lower() + "_tests"


def execution_targets(root):
    return sorted(execution_target(path.stem) for path in (root / EXECUTION).glob("*.cpp"))


def known_targets(root):
    known = set(execution_targets(root)) | {path.stem for path in (root / FRAGMENTS).glob("*.cmake")}
    revisions = ["HEAD"]
    for head in ("REBASE_HEAD", "CHERRY_PICK_HEAD", "MERGE_HEAD"):
        if git(root, "rev-parse", "-q", "--verify", head, check=False).strip():
            revisions += [head, head + "^", git(root, "merge-base", "HEAD", head, check=False).strip()]
    for revision in filter(None, revisions):
        listing = git(root, "ls-tree", "--name-only", f"{revision}:{EXECUTION}", check=False)
        known |= {execution_target(Path(name).stem) for name in listing.splitlines() if name.endswith(".cpp")}
    return sorted(known)


def is_split(text):
    return INCLUDE[0] in text


def git(root, *args, check=True):
    return subprocess.run(["git", *args], cwd=root, capture_output=True, text=True, encoding="utf-8", errors="replace",
                          check=check).stdout


def write_lf(path, text):
    path.parent.mkdir(parents=True, exist_ok=True)
    with open(path, "w", encoding="utf-8", newline="\n") as file:
        file.write(text)


def fragment_path(name):
    return f"{FRAGMENTS}/{name}.cmake"


def command_refactor(root):
    text = (root / LISTS).read_text(encoding="utf-8")
    changed = refactor(text)
    if changed == text:
        sys.exit(f"{LISTS} already registers its default tests through agc_add_default_test")
    write_lf(root / LISTS, changed)
    print(LISTS)


def command_split(root):
    text = (root / LISTS).read_text(encoding="utf-8")
    if is_split(text):
        sys.exit(f"{LISTS} already includes {FRAGMENTS}")
    lists, fragments = migrated(text, execution_targets(root))
    existing = sorted(set(fragments) & {path.stem for path in (root / FRAGMENTS).glob("*.cmake")})
    if existing:
        sys.exit(f"{FRAGMENTS} already has {', '.join(name + '.cmake' for name in existing)}")
    for name, body in fragments.items():
        write_lf(root / fragment_path(name), body)
    write_lf(root / LISTS, lists)
    print(f"{len(fragments)} fragments in {FRAGMENTS}, {len(lists.splitlines())} lines left in {LISTS}")


def merge(base, ours, theirs, label):
    if base == theirs:
        return ours, False
    if base == ours:
        return theirs, False
    with tempfile.TemporaryDirectory() as directory:
        paths = []
        for name, text in (("ours", ours), ("base", base), ("theirs", theirs)):
            write_lf(Path(directory) / name, text)
            paths.append(str(Path(directory) / name))
        result = subprocess.run(["git", "merge-file", "-p", "-L", f"{label} (split)", "-L", "before the split",
                                 "-L", f"{label} (change)", *paths], capture_output=True, text=True, encoding="utf-8")
    if not 0 <= result.returncode < 128:
        raise SplitError(f"git merge-file failed on {label}")
    return result.stdout, result.returncode > 0


def apply_change(base, change, lists, read, execution=()):
    base_lists, base_fragments = migrated(base, execution)
    change_lists, change_fragments = migrated(change, execution)
    written, conflicted = {}, []
    merged, clash = merge(base_lists, lists, change_lists, LISTS)
    if merged != lists:
        written[LISTS] = merged
    if clash:
        conflicted.append(LISTS)
    for name in sorted(set(base_fragments) | set(change_fragments)):
        before, after = base_fragments.get(name, ""), change_fragments.get(name, "")
        if before == after:
            continue
        path = fragment_path(name)
        merged, clash = merge(before, read(path), after, path)
        written[path] = merged
        if clash:
            conflicted.append(path)
    return written, conflicted


def command_resolve(root):
    stages = {}
    for line in git(root, "ls-files", "-u", "--", LISTS).splitlines():
        meta, _ = line.split("\t", 1)
        _, sha, stage = meta.split()
        stages[stage] = sha
    if not {"1", "2", "3"} <= set(stages):
        sys.exit(f"{LISTS} is not in a three-way conflict")
    base, ours, theirs = (git(root, "cat-file", "-p", stages[stage]) for stage in "123")
    if is_split(base) or is_split(ours) == is_split(theirs):
        sys.exit(f"{LISTS}: the conflict is not between a change made before the split and the split; resolve it by hand")
    lists, change = (ours, theirs) if is_split(ours) else (theirs, ours)

    def read(path):
        file = root / path
        return file.read_text(encoding="utf-8") if file.exists() else ""

    try:
        written, conflicted = apply_change(base, change, lists, read, known_targets(root))
    except SplitError as error:
        sys.exit(f"{LISTS}: {error}")
    written.setdefault(LISTS, lists)
    for path, text in written.items():
        if text.strip():
            write_lf(root / path, text)
        elif (root / path).exists():
            (root / path).unlink()
    clean = [path for path in written if path not in conflicted]
    if clean:
        git(root, "add", "-A", "--", *clean)
    print("\n".join(sorted(written)))
    if conflicted:
        sys.exit("resolve the conflict markers in " + ", ".join(conflicted) + ", then git add them")


def main(argv=None):
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, "reconfigure"):
            stream.reconfigure(encoding="utf-8")
    parser = argparse.ArgumentParser(description=f"Move the AGC test registrations of {LISTS} into one fragment per test executable, "
                                                 "and move registrations of a change made before the split")
    parser.add_argument("command", choices=("refactor", "split", "resolve"),
                        help="refactor: register default, draw-pipeline and SPIR-V mode tests through functions; "
                             f"split: refactor, then move the registrations into {FRAGMENTS}/; "
                             f"resolve: during a rebase or merge stopped on {LISTS}, apply the change's registrations to the "
                             "fragments and stage them")
    args = parser.parse_args(argv)
    root = Path(git(Path.cwd(), "rev-parse", "--show-toplevel").strip())
    try:
        {"refactor": command_refactor, "split": command_split, "resolve": command_resolve}[args.command](root)
    except SplitError as error:
        sys.exit(f"{LISTS}: {error}")


if __name__ == "__main__":
    main()
