import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
import re
import shutil
import sys
import tempfile
import subprocess
from pathlib import Path

import compare_results
import hw_oracle


HERE = Path(__file__).resolve().parent
DEFAULT_CATALOG = HERE / "probes.catalog"
MODE_LIMITS = {"ieee": 2, "denorm32": 4, "denorm16": 4, "dx10_clamp": 2,
               "round32": 4, "round16": 4, "fp16_overflow": 2}
PROBE_KEYS = {"id", "instruction", "description", "body", "output_registers", "wave_size",
              "floating_point_modes", "lds_bytes", "memory", "rows"}
CATALOG_KEYS = {"schema_version", "probes"}
ROW_KEYS = {"name", "input"}
IDENTIFIER = re.compile(r"[a-z0-9]+(?:-[a-z0-9]+)*\Z")
INSTRUCTION = re.compile(r"[a-z][a-z0-9_]*\Z")
DECIMAL_WORD = re.compile(r"[0-9]+\Z")
HEX_WORD = re.compile(r"0[xX][0-9a-fA-F]+\Z")


def parse_word(value, context):
    if type(value) is int:
        number = value
    elif isinstance(value, str) and (DECIMAL_WORD.fullmatch(value) or HEX_WORD.fullmatch(value)):
        try:
            number = int(value, 0)
        except ValueError as error:
            raise ValueError(f"{context} must be an unsigned 32-bit integer or numeric string") from error
    else:
        raise ValueError(f"{context} must be an unsigned 32-bit integer or numeric string")
    if number < 0 or number > 0xffffffff:
        raise ValueError(f"{context} must be an unsigned 32-bit integer")
    return number


def validate_probe(probe, index):
    context = f"probe {index}"
    if not isinstance(probe, dict):
        raise ValueError(f"{context} must be an object")
    if set(probe) != PROBE_KEYS:
        missing = sorted(PROBE_KEYS - probe.keys())
        extra = sorted(probe.keys() - PROBE_KEYS)
        details = []
        if missing:
            details.append("missing " + ", ".join(missing))
        if extra:
            details.append("unknown " + ", ".join(extra))
        raise ValueError(f"{context}: " + "; ".join(details))
    probe_id = probe["id"]
    if not isinstance(probe_id, str) or not IDENTIFIER.fullmatch(probe_id):
        raise ValueError(f"{context} id must contain lowercase letters, digits and single hyphens")
    instruction = probe["instruction"]
    if not isinstance(instruction, str) or not INSTRUCTION.fullmatch(instruction):
        raise ValueError(f"{context} instruction must be an assembly mnemonic")
    description = probe["description"]
    if not isinstance(description, str) or not description.strip():
        raise ValueError(f"probe {probe_id} description must be nonempty text")
    body = probe["body"]
    body_lines = [line.strip() for line in body.splitlines() if line.strip()] if isinstance(body, str) else []
    if len(body_lines) != 1 or body_lines[0].split()[0] != instruction or any(
            marker in body_lines[0] for marker in ("#", ";")):
        raise ValueError(f"probe {probe_id} body must contain one uncommented declared instruction")
    output_registers = probe["output_registers"]
    if type(output_registers) is not int or output_registers not in range(1, 17):
        raise ValueError(f"probe {probe_id} output_registers must be from 1 to 16")
    wave_size = probe["wave_size"]
    if type(wave_size) is not int or wave_size not in (32, 64):
        raise ValueError(f"probe {probe_id} wave_size must be 32 or 64")
    modes = probe["floating_point_modes"]
    if not isinstance(modes, dict) or modes.keys() != MODE_LIMITS.keys():
        raise ValueError(f"probe {probe_id} floating_point_modes must define every hardware-oracle mode")
    for name, limit in MODE_LIMITS.items():
        if type(modes[name]) is not int or modes[name] not in range(limit):
            raise ValueError(f"probe {probe_id} mode {name} must be from 0 to {limit - 1}")
    lds_bytes = probe["lds_bytes"]
    if type(lds_bytes) is not int or lds_bytes not in range(hw_oracle.MAX_LDS + 1):
        raise ValueError(f"probe {probe_id} lds_bytes must be from 0 to {hw_oracle.MAX_LDS}")
    if probe["memory"] not in ("fine", "coarse"):
        raise ValueError(f"probe {probe_id} memory must be fine or coarse")
    rows = probe["rows"]
    if not isinstance(rows, list) or not rows:
        raise ValueError(f"probe {probe_id} rows must be a nonempty array")
    row_names = set()
    normalized_rows = []
    for row_index, row in enumerate(rows, 1):
        row_context = f"probe {probe_id} row {row_index}"
        if not isinstance(row, dict) or set(row) != ROW_KEYS:
            raise ValueError(f"{row_context} must contain exactly name and input")
        name = row["name"]
        if not isinstance(name, str) or not IDENTIFIER.fullmatch(name):
            raise ValueError(f"{row_context} name must contain lowercase letters, digits and single hyphens")
        if name in row_names:
            raise ValueError(f"probe {probe_id} has duplicate row name {name}")
        row_names.add(name)
        values = row["input"]
        if not isinstance(values, list) or len(values) != 4:
            raise ValueError(f"{row_context} input must contain four dwords")
        normalized_rows.append({"name": name, "input": [
            parse_word(value, f"{row_context} input word {word_index}")
            for word_index, value in enumerate(values, 1)
        ]})
    return {**probe, "rows": normalized_rows}


def read_catalog(path=DEFAULT_CATALOG):
    try:
        raw = Path(path).read_bytes()
        catalog = json.loads(raw.decode("utf-8-sig"))
    except RecursionError as error:
        raise ValueError(f"{path}: catalog is nested too deeply") from error
    except (OSError, UnicodeError, json.JSONDecodeError) as error:
        raise ValueError(f"{path}: cannot read probe catalog: {error}") from error
    if not isinstance(catalog, dict) or set(catalog) != CATALOG_KEYS:
        raise ValueError(f"{path}: expected an object with schema_version and probes")
    if type(catalog["schema_version"]) is not int or catalog["schema_version"] != 1:
        raise ValueError(f"{path}: unsupported schema_version")
    probes = catalog["probes"]
    if not isinstance(probes, list) or not probes:
        raise ValueError(f"{path}: probes must be a nonempty array")
    validated = []
    identifiers = set()
    for index, item in enumerate(probes, 1):
        probe = validate_probe(item, index)
        if probe["id"] in identifiers:
            raise ValueError(f"{path}: duplicate probe id {probe['id']}")
        identifiers.add(probe["id"])
        validated.append(probe)
    return {"schema_version": 1, "probes": validated}, hashlib.sha256(raw).hexdigest()


def select_probes(probes, identifiers):
    if not identifiers:
        return list(probes)
    by_id = {probe["id"]: probe for probe in probes}
    selected = []
    seen = set()
    for probe_id in identifiers:
        if probe_id not in by_id:
            raise ValueError(f"unknown probe id: {probe_id}")
        if probe_id in seen:
            raise ValueError(f"probe selected more than once: {probe_id}")
        seen.add(probe_id)
        selected.append(by_id[probe_id])
    return selected


def write_atomic(path, content):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    descriptor, temporary = tempfile.mkstemp(prefix=f".{path.name}.", dir=path.parent)
    try:
        with os.fdopen(descriptor, "w", encoding="utf-8", newline="\n") as stream:
            stream.write(content)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary, path)
    except BaseException:
        try:
            os.unlink(temporary)
        except FileNotFoundError:
            pass
        raise


def render_probe_metadata(probe, catalog_digest):
    return {
        "schema_version": 1,
        "catalog_sha256": catalog_digest,
        "probe_sha256": probe_digest(probe),
        "id": probe["id"],
        "instruction": probe["instruction"],
        "description": probe["description"],
        "body": probe["body"],
        "output_registers": probe["output_registers"],
        "wave_size": probe["wave_size"],
        "floating_point_modes": probe["floating_point_modes"],
        "lds_bytes": probe["lds_bytes"],
        "memory": probe["memory"],
        "rows": probe["rows"],
    }


def probe_digest(probe):
    encoded = json.dumps(probe, sort_keys=True, separators=(",", ":"), ensure_ascii=False).encode("utf-8")
    return hashlib.sha256(encoded).hexdigest()


def generate(probes, catalog_digest, output_dir):
    root = Path(output_dir)
    root.mkdir(parents=True, exist_ok=True)
    destinations = [root / probe["id"] for probe in probes]
    existing = [str(path) for path in destinations if path.exists()]
    if existing:
        raise ValueError("refusing to replace generated probe directories: " + ", ".join(existing))
    stage = Path(tempfile.mkdtemp(prefix=".probe-catalog-", dir=root))
    try:
        for probe in probes:
            case = stage / probe["id"]
            case.mkdir()
            metadata = render_probe_metadata(probe, catalog_digest)
            (case / "probe.json").write_text(json.dumps(metadata, indent=2) + "\n", encoding="utf-8")
            (case / "body.s").write_text(probe["body"] + "\n", encoding="utf-8")
            rows = "".join(" ".join(f"0x{word:08x}" for word in row["input"]) + "\n"
                           for row in probe["rows"])
            (case / "rows.txt").write_text(rows, encoding="utf-8")
        promoted = []
        try:
            for probe, destination in zip(probes, destinations):
                os.replace(stage / probe["id"], destination)
                promoted.append(destination)
        except OSError as promotion_error:
            rollback_errors = []
            for destination in reversed(promoted):
                try:
                    shutil.rmtree(destination)
                except OSError as error:
                    rollback_errors.append(error)
            if rollback_errors:
                details = "; ".join(str(error) for error in rollback_errors)
                raise RuntimeError(f"Probe catalog promotion failed: {promotion_error}; rollback failed: {details}") from promotion_error
            raise
    finally:
        shutil.rmtree(stage, ignore_errors=True)
    return destinations


def list_probes(probes):
    for probe in probes:
        print(f"{probe['id']}\t{probe['instruction']}\t{len(probe['rows'])} rows\t{probe['description']}")


def validate_device_label(value):
    if not isinstance(value, str) or not IDENTIFIER.fullmatch(value):
        raise ValueError("device label must contain lowercase letters, digits and single hyphens")
    return value


def make_result(probe, digest, device_label, target, runtime, outputs):
    modes = probe["floating_point_modes"]
    return {
        "schema_version": 1,
        "runtime": runtime,
        "assembly_target": target,
        "wave_size": probe["wave_size"],
        "floating_point_modes": modes,
        "lds_bytes": probe["lds_bytes"],
        "memory": probe["memory"],
        "body": probe["body"],
        "extra_bytes": 0,
        "extra_sha256": hashlib.sha256(b"").hexdigest(),
        "output_registers": [f"v{10 + index}" for index in range(probe["output_registers"])],
        "rows": [
            {"input": row["input"], "output": list(output[:probe["output_registers"]])}
            for row, output in zip(probe["rows"], outputs)
        ],
        "probe_id": probe["id"],
        "probe_instruction": probe["instruction"],
        "probe_description": probe["description"],
        "probe_catalog_sha256": digest,
        "probe_sha256": probe_digest(probe),
        "device_label": device_label,
        "measured_at_utc": datetime.now(timezone.utc).isoformat(timespec="seconds"),
    }


def run_probes(probes, digest, output_dir, device_label):
    device_label = validate_device_label(device_label)
    directory = Path(output_dir) / device_label
    destinations = [directory / f"{probe['id']}.json" for probe in probes]
    existing = [str(path) for path in destinations if path.exists()]
    if existing:
        raise ValueError("refusing to replace existing measurement reports: " + ", ".join(existing))
    directory.mkdir(parents=True, exist_ok=True)
    runtime = hw_oracle.runtime()
    target = hw_oracle.target()
    for probe, destination in zip(probes, destinations):
        rows = [tuple(row["input"]) for row in probe["rows"]]
        outputs = hw_oracle.run(
            probe["body"], rows, wave64=probe["wave_size"] == 64,
            coarse=probe["memory"] == "coarse", **probe["floating_point_modes"],
            lds=probe["lds_bytes"],
        )
        if len(outputs) != len(rows) or any(len(row) < probe["output_registers"] for row in outputs):
            raise ValueError(f"hardware oracle returned an incomplete result for {probe['id']}")
        if any(type(word) is not int or word < 0 or word > 0xffffffff
               for output in outputs for word in output[:probe["output_registers"]]):
            raise ValueError(f"hardware oracle returned a non-dword result for {probe['id']}")
        report = make_result(probe, digest, device_label, target, runtime, outputs)
        write_atomic(destination, json.dumps(report, indent=2) + "\n")
        print(f"Measured {probe['id']}: {destination}")
    return destinations


def read_measurement(path, probe, device_label):
    try:
        report = compare_results.read_report(path)
    except (OSError, UnicodeError, ValueError) as error:
        return "invalid", str(error)
    if report.get("probe_sha256") != probe_digest(probe):
        return "stale", "probe definition digest differs"
    expected = {
        "probe_id": probe["id"],
        "probe_instruction": probe["instruction"],
        "probe_description": probe["description"],
        "device_label": device_label,
        "body": probe["body"],
        "wave_size": probe["wave_size"],
        "floating_point_modes": probe["floating_point_modes"],
        "lds_bytes": probe["lds_bytes"],
        "memory": probe["memory"],
        "extra_bytes": 0,
        "extra_sha256": hashlib.sha256(b"").hexdigest(),
        "output_registers": [f"v{10 + index}" for index in range(probe["output_registers"])],
    }
    for name, value in expected.items():
        if report.get(name) != value:
            return "invalid", f"{name} differs from the curated probe"
    expected_inputs = [row["input"] for row in probe["rows"]]
    observed_inputs = [row["input"] for row in report["rows"]]
    if observed_inputs != expected_inputs:
        return "invalid", "input rows differ from the curated probe"
    if report.get("device_label") != device_label:
        return "invalid", "device label differs from the selected label"
    if report.get("runtime") not in hw_oracle.RUNTIMES or not report.get("assembly_target", "").strip():
        return "invalid", "runtime or assembly target is missing"
    return "measured", ""


def coverage(probes, digest, results_dir, device_label):
    device_label = validate_device_label(device_label)
    result = []
    for probe in probes:
        path = Path(results_dir) / device_label / f"{probe['id']}.json"
        if not path.exists():
            result.append({"id": probe["id"], "status": "missing", "detail": "no report"})
            continue
        status, detail = read_measurement(path, probe, device_label)
        result.append({"id": probe["id"], "status": status, "detail": detail})
    return result


def print_coverage(probes, digest, results, device_label, as_json=False):
    measured = sum(item["status"] == "measured" for item in results)
    if as_json:
        print(json.dumps({
            "device_label": device_label,
            "catalog_sha256": digest,
            "measured": measured,
            "total": len(probes),
            "percent": round(100 * measured / len(probes), 1),
            "probes": results,
        }, indent=2))
        return
    percent = 100 * measured / len(probes)
    print(f"Device label: {device_label} (user-supplied; physical identity is not verified)")
    print(f"Measured coverage: {measured}/{len(probes)} ({percent:.1f}%)")
    print(f"Catalog SHA-256: {digest}")
    for item in results:
        suffix = f" — {item['detail']}" if item["detail"] else ""
        print(f"{item['status']}: {item['id']}{suffix}")


def build_parser():
    parser = argparse.ArgumentParser(
        description="Generate and run only the instruction probes explicitly listed in a curated catalog.")
    commands = parser.add_subparsers(dest="command", required=True)
    listing = commands.add_parser("list", help="list the curated instruction probes")
    listing.add_argument("--catalog", type=Path, default=DEFAULT_CATALOG)
    for name, help_text in (("generate", "write reproducible assembly and input files"),
                            ("run", "measure selected probes on the configured hardware oracle"),
                            ("coverage", "report measured and missing probes for a device label")):
        command = commands.add_parser(name, help=help_text)
        command.add_argument("--catalog", type=Path, default=DEFAULT_CATALOG)
        command.add_argument("--probe", action="append", dest="probe_ids",
                             help="select a probe id; repeat to select more than one (default: all)")
        if name == "generate":
            command.add_argument("--output-dir", type=Path, required=True)
        else:
            command.add_argument("--results-dir", type=Path, required=True)
            command.add_argument("--device-label", required=True,
                                 help="user label for the measurement set; not checked against physical hardware")
            if name == "coverage":
                command.add_argument("--json", action="store_true", help="print machine-readable coverage data")
    return parser


def main(argv=None):
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, "reconfigure"):
            stream.reconfigure(encoding="utf-8", errors="backslashreplace")
    parser = build_parser()
    args = parser.parse_args(argv)
    try:
        catalog, digest = read_catalog(args.catalog)
        probes = catalog["probes"]
        if args.command == "list":
            list_probes(probes)
            return 0
        selected = select_probes(probes, args.probe_ids)
        if args.command == "generate":
            for path in generate(selected, digest, args.output_dir):
                print(path)
            return 0
        if args.command == "run":
            run_probes(selected, digest, args.results_dir, args.device_label)
            return 0
        states = coverage(selected, digest, args.results_dir, args.device_label)
        print_coverage(selected, digest, states, args.device_label, as_json=args.json)
        return int(any(item["status"] != "measured" for item in states))
    except subprocess.CalledProcessError as error:
        parser.error(f"hardware oracle command failed with status {error.returncode}: {error.cmd}")
    except (OSError, ValueError) as error:
        parser.error(str(error))


if __name__ == "__main__":
    raise SystemExit(main())
