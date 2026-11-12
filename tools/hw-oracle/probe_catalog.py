import argparse
import hashlib
import json
import os
import re
import sys
import tempfile
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


def parse_word(value, context):
    if type(value) is int:
        number = value
    elif isinstance(value, str):
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
    if not isinstance(body, str) or not body.strip() or body.split()[0] != instruction:
        raise ValueError(f"probe {probe_id} body must start with its declared instruction")
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
        for probe, destination in zip(probes, destinations):
            os.replace(stage / probe["id"], destination)
    finally:
        try:
            stage.rmdir()
        except OSError:
            pass
    return destinations


def list_probes(probes):
    for probe in probes:
        print(f"{probe['id']}\t{probe['instruction']}\t{len(probe['rows'])} rows\t{probe['description']}")
