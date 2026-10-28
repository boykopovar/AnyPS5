import argparse
import json
import re
from pathlib import Path


SETTINGS = ("body", "wave_size", "floating_point_modes", "lds_bytes", "memory",
            "extra_bytes", "extra_sha256", "output_registers")
MODES = dict(ieee=2, denorm32=4, denorm16=4, dx10_clamp=2, round32=4, round16=4, fp16_overflow=2)


def integer(value, minimum, maximum):
    return type(value) is int and minimum <= value <= maximum


def read_report(path):
    report = json.loads(path.read_text(encoding="utf-8-sig"))
    if not isinstance(report, dict):
        raise ValueError(f"{path}: expected a JSON object")
    required = {"schema_version", "assembly_target", "rows", *SETTINGS}
    missing = required - report.keys()
    if missing:
        raise ValueError(f"{path}: missing fields: {', '.join(sorted(missing))}")
    if not integer(report["schema_version"], 1, 1):
        raise ValueError(f"{path}: unsupported schema_version")
    modes = report["floating_point_modes"]
    if not isinstance(modes, dict) or modes.keys() != MODES.keys() or any(
            not integer(modes[name], 0, limit - 1) for name, limit in MODES.items()):
        raise ValueError(f"{path}: invalid floating_point_modes")
    if not isinstance(report["body"], str):
        raise ValueError(f"{path}: body must be text")
    if not isinstance(report["assembly_target"], str) or not report["assembly_target"].strip():
        raise ValueError(f"{path}: assembly_target must be nonempty text")
    if type(report["wave_size"]) is not int or report["wave_size"] not in (32, 64):
        raise ValueError(f"{path}: wave_size must be 32 or 64")
    if not integer(report["lds_bytes"], 0, 65536):
        raise ValueError(f"{path}: invalid lds_bytes")
    if report["memory"] not in ("fine", "coarse"):
        raise ValueError(f"{path}: memory must be fine or coarse")
    if type(report["extra_bytes"]) is not int or report["extra_bytes"] < 0:
        raise ValueError(f"{path}: invalid extra_bytes")
    digest = report["extra_sha256"]
    if not isinstance(digest, str) or re.fullmatch(r"[0-9a-f]{64}", digest) is None:
        raise ValueError(f"{path}: invalid extra_sha256")
    registers = report["output_registers"]
    if not isinstance(registers, list) or not 1 <= len(registers) <= 16 or registers != [
            f"v{10 + index}" for index in range(len(registers))]:
        raise ValueError(f"{path}: output_registers must be consecutive registers starting at v10, up to v25")
    rows = report["rows"]
    if not isinstance(rows, list) or not rows:
        raise ValueError(f"{path}: rows must be a nonempty array")
    for index, row in enumerate(rows, 1):
        for name, count in (("input", 4), ("output", len(registers))):
            values = row.get(name) if isinstance(row, dict) else None
            if not isinstance(values, list) or len(values) != count or any(
                    not integer(value, 0, 0xffffffff) for value in values):
                raise ValueError(f"{path}: row {index} {name} needs {count} unsigned 32-bit integers")
    return report


def compare(before, after):
    for name in SETTINGS:
        if before[name] != after[name]:
            raise ValueError(f"incompatible reports: {name} differs")
    if len(before["rows"]) != len(after["rows"]):
        raise ValueError("incompatible reports: row count differs")
    for index, (left, right) in enumerate(zip(before["rows"], after["rows"]), 1):
        if left["input"] != right["input"]:
            raise ValueError(f"incompatible reports: row {index} input differs")
    differences = []
    for index, (left, right) in enumerate(zip(before["rows"], after["rows"]), 1):
        for register, old, new in zip(before["output_registers"], left["output"], right["output"]):
            if old != new:
                differences.append((index, register, old, new))
    return differences


def nonnegative_integer(value):
    number = int(value)
    if number < 0:
        raise argparse.ArgumentTypeError("must be a nonnegative integer")
    return number


def main():
    parser = argparse.ArgumentParser(
        description="Compare two hw_oracle.py --json reports bit for bit without a GPU. "
                    "Inputs and settings must match; assembly targets may differ.",
        epilog="Exit codes: 0 = identical outputs, 1 = different outputs, 2 = invalid or incompatible reports. "
               "Assembly targets do not identify the physical GPU or driver.")
    parser.add_argument("before", type=Path)
    parser.add_argument("after", type=Path)
    parser.add_argument("--max-differences", type=nonnegative_integer, metavar="N",
                        help="print at most N differing words (0 prints only counts); all words are still compared")
    args = parser.parse_args()
    try:
        before, after = read_report(args.before), read_report(args.after)
        differences = compare(before, after)
    except (OSError, UnicodeError, ValueError) as error:
        parser.error(str(error))
    print(f"Assembly targets: {before['assembly_target']} -> {after['assembly_target']}")
    print(f"Compared {len(before['rows'])} rows x {len(before['output_registers'])} registers: "
          f"{len(differences)} differing words")
    visible = differences if args.max_differences is None else differences[:args.max_differences]
    for row, register, old, new in visible:
        print(f"row {row}, {register}: 0x{old:08x} -> 0x{new:08x}")
    if len(visible) < len(differences):
        print(f"{len(differences) - len(visible)} additional differing words omitted")
    return int(bool(differences))


if __name__ == "__main__":
    raise SystemExit(main())
