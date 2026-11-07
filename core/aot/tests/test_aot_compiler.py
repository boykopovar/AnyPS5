import argparse
import os
import random
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

MASK = (1 << 64) - 1


def run(command, success=True):
    result = subprocess.run([str(part) for part in command], capture_output=True, text=True, timeout=20)
    if result.returncode != (0 if success else 1):
        raise AssertionError(f"{command}: exit {result.returncode}\n{result.stdout}\n{result.stderr}")
    return result


def elf(code, name="leaf", elf_type=1, symbol_size=None, relocation=False):
    names = b"\0.text\0.symtab\0.strtab\0.shstrtab\0.rela.text\0"
    strings = b"\0" + name.encode() + b"\0"
    base = 0 if elf_type == 1 else 0x400000
    symbols = bytes(24) + struct.pack("<IBBHQQ", 1, 0x12, 0, 1, base,
                                      len(code) if symbol_size is None else symbol_size)
    contents = [b"", code, symbols, strings, names]
    if relocation:
        contents.append(struct.pack("<QQq", 0, (1 << 32) | 1, 0))
    data = bytearray(64)
    offsets = []
    for content in contents:
        data.extend(bytes((-len(data)) % 8))
        offsets.append(len(data))
        data.extend(content)
    data.extend(bytes((-len(data)) % 8))
    section_offset = len(data)
    sections = [
        (0, 0, 0, 0, 0, 0, 0, 0, 0, 0),
        (1, 1, 6, base, offsets[1], len(code), 0, 0, 1, 0),
        (7, 2, 0, 0, offsets[2], len(symbols), 3, 1, 8, 24),
        (15, 3, 0, 0, offsets[3], len(strings), 0, 0, 1, 0),
        (23, 3, 0, 0, offsets[4], len(names), 0, 0, 1, 0),
    ]
    if relocation:
        sections.append((33, 4, 0, 0, offsets[5], 24, 2, 1, 8, 24))
    for section in sections:
        data.extend(struct.pack("<IIQQQQIIQQ", *section))
    ident = b"\x7fELF\x02\x01\x01" + bytes(9)
    data[:64] = struct.pack("<16sHHIQQQIHHHHHH", ident, elf_type, 62, 1, base, 0,
                            section_offset, 0, 64, 0, 0, 64, len(sections), 4)
    return bytes(data)


def compile_function(options, directory, name, code, arguments=6, opt=2, extra=()):
    source = directory / f"{name}.elf"
    output = directory / f"{name}.o"
    ir = directory / f"{name}.ll"
    assembly = directory / f"{name}.s"
    source.write_bytes(elf(code, name))
    result = run([options.compiler, "--input", source, "--function", name, "--args", arguments,
                  "--output", output, "--emit-llvm", ir, "--emit-asm", assembly, "--opt", opt, *extra])
    if output.stat().st_size == 0 or assembly.stat().st_size == 0:
        raise AssertionError("Empty code output")
    run([options.ir_checker, ir])
    return output, assembly, result


def abi():
    return [(bytes.fromhex(instruction) + b"\xc3", lambda args, i=index: args[i])
            for index, instruction in enumerate(("48 89 f8", "48 89 f0", "48 89 d0",
                                                  "48 89 c8", "4c 89 c0", "4c 89 c8"))]


def arithmetic():
    cases = []
    for width in (32, 64):
        prefix = b"\x48" if width == 64 else b""
        mask = (1 << width) - 1
        for opcode, operation in (
            (0x01, lambda a, b: a + b), (0x09, lambda a, b: a | b),
            (0x21, lambda a, b: a & b), (0x29, lambda a, b: a - b),
            (0x31, lambda a, b: a ^ b),
        ):
            for reverse in (False, True):
                instruction = bytes([opcode + (2 if reverse else 0), 0xc6 if reverse else 0xf0])
                code = prefix + b"\x89\xf8" + prefix + instruction + b"\xc3"
                cases.append((code, lambda args, op=operation, m=mask: op(args[0], args[1]) & m))
        cases.append((prefix + b"\x89\xf8" + prefix + b"\x0f\xaf\xc6\xc3",
                      lambda args, m=mask: (args[0] * args[1]) & m))
        cases.append((prefix + b"\x89\xf8" + prefix + b"\xf7\xd8\xc3",
                      lambda args, m=mask: -args[0] & m))
        cases.append((prefix + b"\x89\xf8" + prefix + b"\xf7\xd0\xc3",
                      lambda args, m=mask: ~args[0] & m))
    return cases


def moves():
    return [
        (b"\x48\xb8" + struct.pack("<Q", 0xfedcba9876543210) + b"\xc3", lambda a: 0xfedcba9876543210),
        (b"\xb8\x2a\0\0\0\xc3", lambda a: 42),
        (b"\x48\xc7\xc0\xff\xff\xff\xff\xc3", lambda a: MASK),
        (b"\xc7\xc0\xff\xff\xff\xff\xc3", lambda a: 0xffffffff),
        (b"\x48\x89\xf8\x89\xf0\xc3", lambda a: a[1] & 0xffffffff),
        (b"\x48\x8b\xc7\xc3", lambda a: a[0]),
        (b"\x49\x89\xfa\x4c\x89\xd0\xc3", lambda a: a[0]),
        (b"\x4c\x89\xc0\x4c\x01\xc8\xc3", lambda a: (a[4] + a[5]) & MASK),
        (b"\x41\xb8\xff\xff\xff\xff\x4c\x89\xc0\xc3", lambda a: 0xffffffff),
        (b"\x48\x63\xc7\xc3", lambda a: ((a[0] & 0x7fffffff) - (a[0] & 0x80000000)) & MASK),
        (b"\x31\xc0\xc3", lambda a: 0),
        (b"\x48\x29\xc0\xc3", lambda a: 0),
        (b"\x48\x89\xf8\x48\x83\xc0\xff\xc3", lambda a: (a[0] - 1) & MASK),
        (b"\x48\x89\xf8\x48\x81\xe8\0\0\0\x80\xc3", lambda a: (a[0] + (1 << 31)) & MASK),
        (b"\x48\x89\xf8\x48\x05\xff\xff\xff\xff\xc3", lambda a: (a[0] - 1) & MASK),
        (b"\x48\x89\xf8\x48\x35\0\0\0\x80\xc3", lambda a: a[0] ^ 0xffffffff80000000),
    ]


def shifts():
    cases = [
        (b"\x48\x89\xf8\xc1\xe0\0\xc3", lambda a: a[0] & 0xffffffff),
        (b"\x48\x89\xf8\xd3\xe0\xc3",
         lambda a: ((a[0] & 0xffffffff) << (a[3] & 31)) & 0xffffffff),
    ]
    for width in (32, 64):
        prefix = b"\x48" if width == 64 else b""
        mask = (1 << width) - 1
        for extension in (4, 5, 7):
            for opcode, count in ((0xd1, 1), (0xc1, 255), (0xd3, None)):
                suffix = bytes([255]) if opcode == 0xc1 else b""
                code = prefix + b"\x89\xf8" + prefix + bytes([opcode, 0xc0 | (extension << 3)]) + suffix + b"\xc3"

                def expected(args, w=width, e=extension, c=count, m=mask):
                    value = args[0] & m
                    shift = (args[3] if c is None else c) & (w - 1)
                    if e == 4:
                        return (value << shift) & m
                    if e == 7 and value & (1 << (w - 1)):
                        value -= 1 << w
                    return (value >> shift) & m

                cases.append((code, expected))
    return cases


def execute_cases(options, directory, cases):
    objects, assemblies, expected_functions = [], [], []
    for index, (code, expected) in enumerate(cases):
        name = f"leaf_{index}"
        output, assembly, _ = compile_function(options, directory, name, code, opt=index % 4)
        objects.append(output)
        assemblies.append(assembly)
        expected_functions.append((name, expected))
    vectors = [[v] * 6 for v in (0, 1, 31, 32, 63, 64, 255, 1 << 31, 1 << 63, MASK)]
    rng = random.Random(9776)
    vectors += [[rng.getrandbits(64) for _ in range(6)] for _ in range(200)]
    reference_assembly = directory / "reference.s"
    reference_source = directory / "reference.cpp"
    assembly_lines = [".text"]
    source_lines = ["#include <cstdint>"]
    parameters = ", ".join(f"uint64_t a{i}" for i in range(6))
    arguments = ", ".join(f"a{i}" for i in range(6))
    for index, (code, _) in enumerate(cases):
        symbol = ("_" if sys.platform == "darwin" else "") + f"guest_{index}"
        assembly_lines += [".p2align 4", f".globl {symbol}", f"{symbol}:",
                           ".byte " + ",".join(str(byte) for byte in code)]
        source_lines += [
            f'extern "C" uint64_t __attribute__((sysv_abi)) guest_{index}({parameters});',
            f'extern "C" uint64_t original_{index}({parameters}) {{ return guest_{index}({arguments}); }}',
        ]
    reference_assembly.write_text("\n".join(assembly_lines) + "\n")
    reference_source.write_text("\n".join(source_lines) + "\n")
    native_expected = execute_program(options, directory, "reference", [reference_source, reference_assembly],
                                      [f"original_{i}" for i in range(len(cases))], vectors)
    for index, (_, expected) in enumerate(cases):
        if native_expected[index] != [expected(arguments) for arguments in vectors]:
            raise AssertionError(f"Reference model disagrees with native x86-64 for case {index}")
    for kind, inputs in (("object", objects), ("assembly", assemblies)):
        actual_results = execute_program(options, directory, kind, inputs,
                                         [name for name, _ in expected_functions], vectors)
        for index, (name, expected) in enumerate(expected_functions):
            for arguments, actual, wanted in zip(vectors, actual_results[index], native_expected[index]):
                if actual != wanted:
                    raise AssertionError(f"{kind} {name}{arguments}: {actual:#x} != {wanted:#x}")
    print(f"Executed {len(cases)} functions from both object and assembly output on {len(vectors)} input vectors")


def execute_program(options, directory, kind, inputs, names, vectors):
    source = directory / f"{kind}_runner.cpp"
    executable = directory / (f"{kind}.exe" if os.name == "nt" else kind)
    parameters = ", ".join(["uint64_t"] * 6)
    arguments = ", ".join(f"a[{i}]" for i in range(6))
    lines = [
        "#if !defined(__x86_64__) && !defined(_M_X64)",
        '#error "Leaf execution tests must be compiled for x86-64"', "#endif",
        "#include <cstdint>", "#include <cstdio>",
        *[f'extern "C" uint64_t {name}({parameters});' for name in names],
        "int main() {",
        f"using Function = uint64_t (*)({parameters});",
        "const Function functions[] = {" + ", ".join(names) + "};",
        "const uint64_t vectors[][6] = {",
        *["{" + ", ".join(f"{value}ULL" for value in vector) + "}," for vector in vectors],
        "};",
        "for (auto function : functions) {",
        "for (const auto& a : vectors) {",
        f'std::printf("%llu\\n", static_cast<unsigned long long>(function({arguments})));',
        "}", "}", "return std::ferror(stdout) ? 1 : 0;", "}",
    ]
    source.write_text("\n".join(lines) + "\n")
    architecture = ["-arch", "x86_64"] if sys.platform == "darwin" else []
    run([options.host_compiler, *architecture, "-std=c++20", "-o", executable, source, *inputs])
    try:
        result = run([executable])
    except OSError as error:
        hint = " Apple Silicon requires Rosetta." if sys.platform == "darwin" else ""
        raise AssertionError(f"Cannot launch x86-64 test program.{hint} {error}") from error
    values = [int(line) for line in result.stdout.splitlines()]
    if len(values) != len(names) * len(vectors) or any(value < 0 or value > MASK for value in values):
        raise AssertionError("Test program returned incomplete or invalid uint64_t results")
    return [values[index * len(vectors):(index + 1) * len(vectors)] for index in range(len(names))]


def reject(options, directory):
    bad_code = [
        b"", b"\xc3", b"\x48", b"\xb8\x01", b"\x48\xb8\x01",
        b"\x89", b"\x48\x89\xd8\xc3", b"\x48\x89\xc7\xc3",
        b"\x48\x89\xf8", b"\x31\xc0\xc3\x90", b"\x0f\x05\xc3",
        b"\xe8\0\0\0\0\xc3", b"\xeb\0\xc3", b"\x74\0\xc3",
        b"\x48\x8b\x07\xc3", b"\x50\xc3", b"\x48\x89\xfc\xc3",
        b"\xf3\x90\xc3", b"\x66\x89\xf8\xc3", b"\x49\x90\xc3",
        b"\x48\x89\xf8\x48\x11\xf0\xc3",
        b"\x48\x89\xf8\x48\xd3\xc0\xc3",
        b"\x0f\x57\xc0\xc3",
    ]
    source, output = directory / "input.elf", directory / "output.o"
    base = [options.compiler, "--input", source, "--function", "leaf", "--args", 6, "--output", output]
    sentinel = b"previous valid output"
    invalid_inputs = [elf(code) for code in bad_code] + [
        b"not an ELF", elf(b"\x31\xc0\xc3")[:40],
        elf(b"\x31\xc0\xc3", symbol_size=100),
        elf(b"\x31\xc0\xc3", relocation=True),
    ]
    wrong_machine = bytearray(elf(b"\x31\xc0\xc3"))
    struct.pack_into("<H", wrong_machine, 18, 183)
    invalid_inputs.append(wrong_machine)
    for location, value in ((6, 0), (20, 0), (52, 0)):
        malformed = bytearray(elf(b"\x31\xc0\xc3"))
        malformed[location] = value
        invalid_inputs.append(malformed)
    overflow = bytearray(elf(b"\x31\xc0\xc3"))
    struct.pack_into("<Q", overflow, 40, MASK - 8)
    invalid_inputs.append(overflow)
    for data in invalid_inputs:
        source.write_bytes(data)
        output.write_bytes(sentinel)
        result = run(base, success=False)
        if not result.stderr.strip() or output.read_bytes() != sentinel:
            raise AssertionError("Rejection must diagnose the error and preserve existing output")
    source.write_bytes(elf(b"\xb8\x2a\0\0\0\xc3"))
    for extra in (
        ["--args", "7"], ["--args", "-1"], ["--args", "1junk"], ["--opt", "4"],
        ["--opt", "bad"], ["--triple", "not-a-target"], ["--function", "missing"],
        ["--unknown", "value"], ["--emit-llvm"], ["--emit-llvm", output],
        ["--output", source], ["--output", directory], ["--output", directory / "missing" / "out.o"],
    ):
        run([*base, *extra], success=False)
    run([options.compiler, "--input", source, "--output", output], success=False)
    if source.with_name(source.name.upper()).exists():
        alias = directory / "alias.o"
        result = run([*base, "--output", alias, "--emit-llvm", directory / "ALIAS.O"], success=False)
        if not result.stderr.strip() or (alias.exists() and alias.read_bytes().startswith(b";")):
            raise AssertionError("Aliased outputs must fail without replacing the object with IR")
    source.write_bytes(elf(b"\x48\x89\xf8\xc3"))
    run([*base, "--args", "0"], success=False)
    print(f"Rejected {len(invalid_inputs)} invalid or unsupported inputs and invalid CLI/output requests")


def formats(options, directory):
    for index, (triple, magic) in enumerate((
        ("x86_64-unknown-linux-gnu", b"\x7fELF"),
        ("x86_64-w64-windows-gnu", b"\x64\x86"),
        ("x86_64-pc-windows-msvc", b"\x64\x86"),
        ("x86_64-apple-darwin", b"\xcf\xfa\xed\xfe"),
    )):
        output, _, _ = compile_function(options, directory, f"format_{index}",
                                        b"\x48\x89\xf8\x48\x83\xc0\x2a\xc3",
                                        extra=["--triple", triple])
        if not output.read_bytes().startswith(magic):
            raise AssertionError(f"Wrong object format for {triple}")
    for elf_type in (2, 3):
        source = directory / f"type_{elf_type}.elf"
        output = directory / f"type_{elf_type}.o"
        source.write_bytes(elf(b"\xb8\x2a\0\0\0\xc3", elf_type=elf_type))
        run([options.compiler, "--input", source, "--function", "leaf", "--args", "0", "--output", output])
    print("Verified ELF, COFF and Mach-O emission and executable/shared ELF symbol addresses")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--compiler", required=True)
    parser.add_argument("--host-compiler", required=True)
    parser.add_argument("--ir-checker", required=True)
    parser.add_argument("--group", required=True)
    options = parser.parse_args()
    if os.name == "nt":
        os.environ["PATH"] = str(Path(options.host_compiler).parent) + os.pathsep + os.environ.get("PATH", "")
    with tempfile.TemporaryDirectory(prefix="anyps5-leaf-") as temporary:
        directory = Path(temporary)
        if options.group == "reject":
            reject(options, directory)
        elif options.group == "formats":
            formats(options, directory)
        else:
            execute_cases(options, directory,
                          {"abi": abi, "arithmetic": arithmetic, "moves": moves, "shifts": shifts}[options.group]())


if __name__ == "__main__":
    main()
