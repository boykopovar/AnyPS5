import argparse
import copy
import hashlib
import json
import math
import os
import re
import sys
import tempfile
from collections import defaultdict
from pathlib import Path


CLASSES = ("implemented", "partial", "stub", "absent", "module")
GAPS = ("absent", "stub", "partial")
ROOT = Path(__file__).resolve().parent.parent
SCHEMA = ROOT / "docs" / "run-report.schema.json"
SCHEMA_WORDS = {"$schema", "$ref", "$defs", "title", "description", "type", "const", "enum", "properties", "required",
                "additionalProperties", "items", "maxItems", "minItems", "minLength", "maxLength", "pattern", "minimum",
                "maximum", "exclusiveMinimum", "maxProperties", "allOf", "if", "then"}


class CensusError(Exception):
    pass


def canonical(value):
    return json.dumps(value, ensure_ascii=True, sort_keys=True, separators=(",", ":"), allow_nan=False)


def fingerprint(data):
    return hashlib.sha256(data).hexdigest()


def unique_keys(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise CensusError(f"duplicate JSON key {key!r}")
        result[key] = value
    return result


def finite_float(value):
    result = float(value)
    if not math.isfinite(result):
        raise CensusError("nonfinite JSON number")
    return result


def reject_constant(value):
    raise CensusError(f"invalid JSON number {value}")


def decode_json(data):
    try:
        return json.loads(data.decode("utf-8-sig"), object_pairs_hook=unique_keys, parse_constant=reject_constant, parse_float=finite_float)
    except (UnicodeError, ValueError, RecursionError) as error:
        raise CensusError(f"invalid UTF-8 JSON: {error}") from error


def require(condition, message):
    if not condition:
        raise CensusError(message)


def string(value, where, nullable=False, empty=False):
    require((nullable and value is None) or (isinstance(value, str) and (empty or bool(value))), f"{where}: expected {'nullable ' if nullable else ''}string")


def integer(value, where, minimum=0):
    require(type(value) is int and value >= minimum, f"{where}: expected integer >= {minimum}")


def strings(value, where):
    require(isinstance(value, list) and all(isinstance(item, str) and item for item in value), f"{where}: expected string list")
    require(len(value) == len(set(value)), f"{where}: duplicate values")


def locator(path):
    text = str(path).replace("\\", "/")
    if Path(path).is_absolute() or re.match(r"^[A-Za-z]:|^//", text) or ".." in text.split("/"):
        return text.rsplit("/", 1)[-1]
    return text.removeprefix("./")


def network_path(path):
    text = str(path).replace("\\", "/")
    return text.startswith("//") and re.match(r"^//[?.]/[A-Za-z]:/", text) is None


def action(kind, **facts):
    return {"kind": kind, **facts}


def sorted_unique(values):
    return [json.loads(value) for value in sorted({canonical(value) for value in values})]


def schema_vocabulary(schema):
    require(isinstance(schema, dict), "bundled native schema is not an object")
    require(not set(schema) - SCHEMA_WORDS, f"unsupported bundled schema keywords: {sorted(set(schema) - SCHEMA_WORDS)}")
    if "$ref" in schema:
        require(schema["$ref"].startswith("#/"), "external schema references are unsupported")
    for key in ("properties", "$defs"):
        for value in schema.get(key, {}).values():
            schema_vocabulary(value)
    for key in ("items", "if", "then"):
        if key in schema:
            schema_vocabulary(schema[key])
    for value in schema.get("allOf", []):
        schema_vocabulary(value)


def validate_schema(value, schema, root, where=""):
    if "$ref" in schema:
        target = root
        for component in schema["$ref"][2:].split("/"):
            target = target[component.replace("~1", "/").replace("~0", "~")]
        validate_schema(value, target, root, where)
    if "type" in schema:
        types = schema["type"] if isinstance(schema["type"], list) else [schema["type"]]
        matches = {"null": value is None, "object": isinstance(value, dict), "array": isinstance(value, list),
                   "string": isinstance(value, str), "boolean": type(value) is bool, "integer": type(value) is int,
                   "number": type(value) in (int, float)}
        require(any(matches.get(kind, False) for kind in types), f"{where or '/'}: expected {'/'.join(types)}")
    if "const" in schema:
        require(type(value) is type(schema["const"]) and value == schema["const"], f"{where}: unsupported value")
    if "enum" in schema:
        require(any(type(value) is type(item) and value == item for item in schema["enum"]), f"{where}: unsupported value")
    if isinstance(value, dict):
        require(not set(schema.get("required", [])) - set(value), f"{where or '/'}: missing fields {sorted(set(schema.get('required', [])) - set(value))}")
        properties = schema.get("properties", {})
        if schema.get("additionalProperties") is False:
            require(not set(value) - set(properties), f"{where}: unsupported fields {sorted(set(value) - set(properties))}")
        for key in value.keys() & properties.keys():
            validate_schema(value[key], properties[key], root, where + "/" + key)
        require("maxProperties" not in schema or len(value) <= schema["maxProperties"], f"{where}: too many properties")
    if isinstance(value, list):
        require(len(value) >= schema.get("minItems", 0) and len(value) <= schema.get("maxItems", len(value)), f"{where}: invalid list length")
        if "items" in schema:
            for index, item in enumerate(value):
                validate_schema(item, schema["items"], root, f"{where}/{index}")
    if isinstance(value, str):
        require(len(value) >= schema.get("minLength", 0) and len(value) <= schema.get("maxLength", len(value)), f"{where}: invalid string length")
        require("pattern" not in schema or re.search(schema["pattern"], value) is not None, f"{where}: invalid string")
    if type(value) in (int, float):
        require(type(value) is int or math.isfinite(value), f"{where}: nonfinite number")
        require("minimum" not in schema or value >= schema["minimum"], f"{where}: below minimum")
        require("maximum" not in schema or value <= schema["maximum"], f"{where}: above maximum")
        require("exclusiveMinimum" not in schema or value > schema["exclusiveMinimum"], f"{where}: below exclusive minimum")
    for branch in schema.get("allOf", []):
        validate_schema(value, branch, root, where)
    if "if" in schema:
        try:
            validate_schema(value, schema["if"], root, where)
        except CensusError:
            return
        if "then" in schema:
            validate_schema(value, schema["then"], root, where)


def validate_audit(document):
    require(isinstance(document, dict), "audit: expected object")
    require("format" not in document, "audit: unsupported format; expected legacy import_audit output")
    rows = document.get("imports")
    require(isinstance(rows, list), "audit/imports: expected list")
    counts = dict.fromkeys(CLASSES, 0)
    references = dict.fromkeys(CLASSES, 0)
    keys = set()
    for index, row in enumerate(rows):
        where = f"audit/imports/{index}"
        require(isinstance(row, dict), f"{where}: expected object")
        for field in ("nid", "library", "class", "references", "providers", "name"):
            require(field in row, f"{where}: missing {field}")
        module_file = row["library"] is None and isinstance(row.get("module"), str)
        string(row["nid"], where + "/nid", empty=module_file)
        require(not any(character.isspace() or ord(character) < 32 or ord(character) == 127 for character in row["nid"]), f"{where}: invalid import symbol")
        string(row["library"], where + "/library", nullable=True, empty=True)
        string(row["name"], where + "/name", nullable=True)
        if "module" in row:
            string(row["module"], where + "/module")
        integer(row["references"], where + "/references", 1)
        strings(row["providers"], where + "/providers")
        require(row["class"] in CLASSES, f"{where}: unknown class")
        for field in ("weak", "library_mismatch", "symbol_mismatch", "name_verified"):
            if field in row:
                require(type(row[field]) is bool, f"{where}/{field}: expected boolean")
        if "unsearched_providers" in row:
            strings(row["unsearched_providers"], where + "/unsearched_providers")
        key = (row["nid"], row["library"], row.get("module"))
        require(key not in keys, f"{where}: duplicate import key")
        keys.add(key)
        counts[row["class"]] += 1
        references[row["class"]] += row["references"]
    for field, expected in (("unique_imports", len(rows)), ("references", sum(references.values()))):
        integer(document.get(field), "audit/" + field)
        require(document[field] == expected, f"audit/{field}: conservation failed")
    for field, expected in (("unique_by_class", counts), ("references_by_class", references)):
        actual = document.get(field)
        require(isinstance(actual, dict) and set(actual) == set(CLASSES), f"audit/{field}: expected all five classes")
        for kind in CLASSES:
            integer(actual[kind], f"audit/{field}/{kind}")
        require(actual == expected, f"audit/{field}: conservation failed")
    if document.get("source_commit") is not None:
        require(isinstance(document["source_commit"], str) and re.fullmatch(r"[0-9a-f]{40}", document["source_commit"]), "audit/source_commit: expected full commit or null")
    require(document.get("source_dirty") is None or type(document["source_dirty"]) is bool, "audit/source_dirty: expected boolean or null")
    for field in ("library_mismatch", "symbol_mismatch"):
        if field in document:
            integer(document[field], "audit/" + field)
            require(document[field] == sum(row.get(field) is True for row in rows), f"audit/{field}: conservation failed")
    for field in ("missing_libraries", "unsearched_providers"):
        if field in document:
            require(isinstance(document[field], dict), f"audit/{field}: expected object")
            for name, count in document[field].items():
                string(name, f"audit/{field} key")
                integer(count, f"audit/{field}/{name}")
    if "unsearched_providers" in document:
        expected = defaultdict(int)
        for row in rows:
            for provider in row.get("unsearched_providers", []):
                expected[provider] += 1
        require(document["unsearched_providers"] == dict(expected), "audit/unsearched_providers: conservation failed")
    require(isinstance(document.get("missing_host_dependencies", []), list), "audit/missing_host_dependencies: expected list")
    for index, entry in enumerate(document.get("missing_host_dependencies", [])):
        require(isinstance(entry, dict), "audit/missing_host_dependencies: expected objects")
        string(entry.get("importer"), f"audit/missing_host_dependencies/{index}/importer")
        require(isinstance(entry.get("chain"), list) and len(entry["chain"]) >= 2 and all(isinstance(item, str) and item for item in entry["chain"]), "audit/missing_host_dependencies: invalid chain")
    require(isinstance(document.get("stops", []), list), "audit/stops: expected list")
    for entry in document.get("stops", []):
        require(isinstance(entry, dict), "audit/stops: expected objects")
        string(entry.get("registry"), "audit/stops/registry")
        for field in ("nid", "library", "name"):
            string(entry.get(field), f"audit/stops/{field}", nullable=True, empty=field == "library")


class Inputs:
    def __init__(self, limit):
        self.limit = limit
        self.paths = [Path(__file__), SCHEMA]
        self.artifacts = {}

    def read(self, reference, base, adapter):
        require(isinstance(reference, dict), "file reference: expected object")
        require(not set(reference) - {"path", "expected_sha256"}, "file reference: unsupported fields")
        path_text = reference.get("path")
        string(path_text, "file reference/path")
        require("://" not in path_text and "\0" not in path_text, "file reference: expected local path")
        require(not network_path(path_text), "file reference: UNC paths are unsupported")
        path = Path(path_text)
        if not path.is_absolute():
            path = base / path
        self.paths.append(path)
        try:
            require(not network_path(path.resolve()), "file reference: resolved UNC path is unsupported")
            with path.open("rb") as handle:
                data = handle.read(self.limit + 1)
        except (OSError, ValueError, RuntimeError) as error:
            raise CensusError(f"{locator(path_text)}: cannot read input ({getattr(error, 'strerror', None) or type(error).__name__})") from error
        require(len(data) <= self.limit, f"{locator(path_text)}: input exceeds --max-input-bytes")
        digest = fingerprint(data)
        expected = reference.get("expected_sha256")
        if expected is not None:
            require(isinstance(expected, str) and re.fullmatch(r"[0-9a-f]{64}", expected), "expected_sha256: expected 64 lowercase hex digits")
            require(digest == expected, f"{locator(path_text)}: expected_sha256 mismatch")
        identifier = "sha256:" + digest
        artifact = self.artifacts.setdefault(identifier, {"id": identifier, "sha256": digest, "bytes": len(data), "locators": [], "adapters": []})
        artifact["locators"] = sorted(set(artifact["locators"]) | {locator(path_text)})
        artifact["adapters"] = sorted(set(artifact["adapters"]) | {adapter})
        return data, identifier


def evidence(artifact, pointer=None, **position):
    return {"artifact_id": artifact, "sha256": artifact.removeprefix("sha256:"), **({"pointer": pointer} if pointer is not None else {}), **position}


def observation(title, kind, proof, **facts):
    value = {"title_key": title["key"], "kind": kind, "evidence_kinds": [kind], "symbol": None, "name": None, "library": None,
             "module": None, "providers": [], "evidence": proof, "build_match": None, "current_status": "unverified", **facts}
    value["id"] = "observation:" + fingerprint(canonical(value).encode("utf-8"))
    return value


def next_actions(row):
    result = []
    if row["kind"] == "import_presence" and row["symbol"] is None:
        result += [action("inspect_evidence"), action("verify_module_imports")]
    elif row["kind"] == "import_presence" and row["class"] in GAPS:
        result += [action("refresh_on_baseline"), action("collect_runtime_evidence"), action("inspect_semantic_debt"), action("check_overlap")]
        if not row["name"]:
            result.append(action("resolve_symbol_name"))
        if len(row["providers"]) != 1:
            result.append(action("disambiguate_provider"))
    elif row["kind"] != "import_presence":
        result += [action("inspect_evidence"), action("refresh_on_baseline")]
        if row["symbol"] is None:
            result.append(action("disambiguate_subject"))
    return sorted_unique(result)


def attach_imports(title, document, artifact, observations, issues):
    title["audit"] = {"artifact_id": artifact, "source_commit": document.get("source_commit"), "source_dirty": document.get("source_dirty"),
                      "producer": None, "counting_policy": None, "provider_build": None, "references": document["references"],
                      "unique_imports": document["unique_imports"], "unique_by_class": document["unique_by_class"], "references_by_class": document["references_by_class"]}
    for index, raw in enumerate(document["imports"]):
        providers = sorted(raw["providers"])
        row = observation(title, "import_presence", evidence(artifact, f"/imports/{index}"), symbol=raw["nid"] or None, name=raw["name"],
                          library=raw["library"], module=raw.get("module"), providers=providers, **{"class": raw["class"]},
                          references=raw["references"], weak=raw.get("weak"), name_verified=raw.get("name_verified"),
                          library_mismatch=raw.get("library_mismatch"), symbol_mismatch=raw.get("symbol_mismatch"),
                          unsearched_providers=raw.get("unsearched_providers"), origin="module-file" if "module" in raw else "registry",
                          provider_status="none" if not providers else "singleton-reported" if len(providers) == 1 else "ambiguous-reported")
        row["evidence_kinds"] = ["import_presence", "reported_static_class"]
        if not raw["nid"]:
            row["raw_symbol"] = ""
            issues.append({"kind": "unnamed_module_import", "title_key": title["key"], "module": raw.get("module"), "evidence": row["evidence"], "next_actions": [action("verify_module_imports")]})
        row["next_actions"] = next_actions(row)
        observations.append(row)
        for field in ("library_mismatch", "symbol_mismatch"):
            if raw.get(field) is True:
                issues.append({"kind": field, "title_key": title["key"], "symbol": raw["nid"], "evidence": row["evidence"], "next_actions": [action("verify_provider_resolution")]})
        for provider in raw.get("unsearched_providers", []):
            issues.append({"kind": "unsearched_provider", "title_key": title["key"], "symbol": raw["nid"], "provider": provider, "evidence": row["evidence"], "next_actions": [action("verify_provider_resolution")]})
    for library, count in document.get("missing_libraries", {}).items():
        pointer = "/missing_libraries/" + library.replace("~", "~0").replace("/", "~1")
        issues.append({"kind": "missing_library", "title_key": title["key"], "library": library, "references": count, "declared_only": count == 0,
                       "evidence": evidence(artifact, pointer), "next_actions": [action("verify_package_inputs")]})
    for index, entry in enumerate(document.get("missing_host_dependencies", [])):
        issues.append({"kind": "missing_host_dependency", "title_key": title["key"], **entry, "evidence": evidence(artifact, f"/missing_host_dependencies/{index}"), "next_actions": [action("verify_package_inputs")]})
    for index, stop in enumerate(document.get("stops", [])):
        if stop["nid"] is not None:
            issues.append({"kind": "static_unresolved_import", "title_key": title["key"], "symbol": stop["nid"], "library": stop["library"],
                           "name": stop["name"], "evidence": evidence(artifact, f"/stops/{index}"), "next_actions": [action("verify_provider_resolution")]})


def runtime_subject(function, imports):
    if function["library"] is None:
        return None, []
    library = function["library"] + ".prx"
    candidates = [row for row in imports if row["symbol"] is not None and row["name"] == function["name"] and row["name_verified"] is not False
                  and row["providers"] == [library] and row["class"] != "module"]
    symbols = sorted({row["symbol"] for row in candidates})
    return (symbols[0], [library]) if len(symbols) == 1 else (None, [])


def attach_runtime(title, document, artifact, observations):
    run = document["run"]
    lower = bool(run["incomplete"] or run["recovered"] or document["truncated"]["functions"])
    covered = document["coverage"]["missing"]
    title["runs"].append({"artifact_id": artifact, "id": run["id"], "content_version": document["game"]["contentVersion"],
                          "target_platform": document["system"]["os"], "build": document["anyps5"], "build_match": None,
                          "exit": run["exit"], "recovered": run["recovered"], "incomplete": run["incomplete"],
                          "missing_coverage": covered, "truncated_functions": document["truncated"]["functions"],
                          "observation_scope": "lower-bound" if lower else "this-run-only" if covered else "unavailable"})
    imports = [row for row in observations if row["title_key"] == title["key"] and row["kind"] == "import_presence"]
    for index, function in enumerate(document["missing"]["functions"]):
        symbol, providers = runtime_subject(function, imports)
        row = observation(title, "observed_unimplemented_call", evidence(artifact, f"/missing/functions/{index}"), symbol=symbol,
                          name=function["name"], library=function["library"], providers=providers, calls=function["calls"], run_id=run["id"],
                          coverage=covered, lower_bound=lower, attribution="unambiguous-reported-library-name" if symbol else "unassigned-import-identity")
        row["next_actions"] = next_actions(row)
        observations.append(row)
    stop = document["stop"]
    if stop["kind"] != "none":
        matches = []
        if stop["kind"] == "missing-function" and stop["reason"] == "runtime-stop" and run["exit"] == "abort":
            for function in document["missing"]["functions"]:
                if function["library"] is not None:
                    message = f"Called {function['library']} {function['name']}, which is not implemented."
                    if stop["message"] == message:
                        matches.append(function)
        identities = {(entry["library"], entry["name"]) for entry in matches}
        attributed = len(identities) == 1
        function = matches[0] if attributed else None
        symbol, providers = runtime_subject(function, imports) if function else (None, [])
        row = observation(title, "native_attributed_stop" if attributed else "unassigned_runtime_stop", evidence(artifact, "/stop"),
                          symbol=symbol, name=function["name"] if function else None, library=function["library"] if function else None,
                          providers=providers, run_id=run["id"], stop=stop, lower_bound=lower,
                          attribution="native-v1-canonical-message" if attributed else "unassigned", verification_method="native-report-v1")
        row["next_actions"] = next_actions(row)
        observations.append(row)


def validate_claim(claim, data):
    require(isinstance(claim, dict), "stop_claim: expected object")
    require(not set(claim) - {"log", "line_start", "line_end", "excerpt", "subject", "label"}, "stop_claim: unsupported fields")
    integer(claim.get("line_start"), "stop_claim/line_start", 1)
    integer(claim.get("line_end"), "stop_claim/line_end", 1)
    require(claim["line_end"] >= claim["line_start"], "stop_claim: reversed line range")
    string(claim.get("excerpt"), "stop_claim/excerpt")
    try:
        lines = data.decode("utf-8-sig").splitlines()
    except UnicodeError as error:
        raise CensusError("stop_claim: log is not UTF-8 text") from error
    require(claim["line_end"] <= len(lines), "stop_claim: line range outside log")
    excerpt = "\n".join(lines[claim["line_start"] - 1:claim["line_end"]])
    supplied = claim["excerpt"].replace("\r\n", "\n")
    require(excerpt == supplied, "stop_claim: excerpt does not match selected lines")
    if "label" in claim:
        string(claim["label"], "stop_claim/label")
    subject = claim.get("subject")
    if subject is not None:
        require(isinstance(subject, dict), "stop_claim/subject: expected object")
        require(not set(subject) - {"kind", "name", "symbol", "library"}, "stop_claim/subject: unsupported fields")
        kind = subject.get("kind", "function" if "symbol" in subject else "unassigned")
        require(kind in ("function", "function-name", "operation", "unassigned"), "stop_claim/subject: invalid kind")
        for field in ("name", "symbol", "library"):
            if field in subject:
                string(subject[field], "stop_claim/subject/" + field, nullable=field == "library")
        require(kind == "function" or "symbol" not in subject, "stop_claim: only function subject can declare import symbol")
        require(kind not in ("function", "function-name") or bool(subject.get("name") or subject.get("symbol")), "stop_claim: function subject requires name or symbol")
        subject = {"kind": kind, **subject}
    return subject


def title_metadata(entry):
    require(isinstance(entry, dict), "title: expected object")
    require(not set(entry) - {"title_id", "guest_platform", "target_platform", "content_version", "identity_source", "audit", "runtime_reports", "stop_claims"}, "title: unsupported fields")
    identifier, guest = entry.get("title_id"), entry.get("guest_platform")
    require(isinstance(identifier, str) and re.fullmatch(r"[A-Z]{4}[0-9]{5}", identifier), "title_id: expected four uppercase letters and five digits")
    require(guest in ("ps4", "ps5", "unknown"), "guest_platform: expected ps4, ps5 or unknown")
    target = entry.get("target_platform", "unknown")
    require(target in ("windows", "linux", "macos", "unknown"), "target_platform: invalid platform")
    for field in ("content_version", "identity_source"):
        if entry.get(field) is not None:
            string(entry[field], "title/" + field)
    for field in ("runtime_reports", "stop_claims"):
        require(isinstance(entry.get(field, []), list), f"title/{field}: expected list")
    require(entry.get("audit") is not None or entry.get("runtime_reports") or entry.get("stop_claims"), "title: no selected evidence")
    return {"key": guest + ":" + identifier, "title_id": identifier, "guest_platform": guest, "target_platform": target,
            "content_version": entry.get("content_version"), "identity_source": entry.get("identity_source"), "audit": None, "runs": [],
            "runtime_coverage": "unavailable", "build_match": None, "provenance_gaps": ["built_provider_identity_unrecorded", "audit_producer_identity_unrecorded"]}


def runtime_association(title, document):
    identifier = document["game"]["titleId"]
    require(identifier is None or identifier == title["title_id"], "native report titleId conflicts with declared title")
    target = document["system"]["os"]
    require(target is None or title["target_platform"] == "unknown" or target == title["target_platform"], "native report OS conflicts with selected target_platform")
    version = document["game"]["contentVersion"]
    require(title["content_version"] is None or version is None or version == title["content_version"], "native report contentVersion conflicts with selected content_version")


def runtime_coverage(titles):
    selected, covered, complete = [], [], []
    for title in titles:
        runs = title["runs"]
        if runs:
            selected.append(title["key"])
        if any(run["missing_coverage"] for run in runs):
            covered.append(title["key"])
            if all(run["missing_coverage"] and run["observation_scope"] == "this-run-only" for run in runs):
                complete.append(title["key"])
                title["runtime_coverage"] = "complete"
            else:
                title["runtime_coverage"] = "lower-bound"
    return {"selected_runtime_titles": sorted(selected), "covered_runtime_titles": sorted(covered), "complete_runtime_titles": sorted(complete),
            "unknown_titles": sorted(title["key"] for title in titles if title["key"] not in complete)}


def summaries(observations, runtime, manual_titles, grouping):
    grouped = defaultdict(list)
    linked_groups = defaultdict(set)
    for row in observations:
        if row["symbol"] is None:
            continue
        if grouping == "library":
            if row["kind"] != "import_presence":
                continue
            key = (row["symbol"], row["library"])
        elif grouping == "provider":
            if row["kind"] != "import_presence" or len(row["providers"]) != 1:
                continue
            key = (row["symbol"], row["providers"][0])
        else:
            key = (row["symbol"],)
        grouped[key].append(row)
        if grouping != "symbol" and len(row["providers"]) == 1 and row["class"] != "module":
            linked_groups[(row["title_key"], row["symbol"], row["providers"][0])].add(key)
    if grouping != "symbol":
        for row in observations:
            if row["kind"] == "import_presence" or row["symbol"] is None or len(row["providers"]) != 1:
                continue
            for key in linked_groups.get((row["title_key"], row["symbol"], row["providers"][0]), ()):
                grouped[key].append(row)
    result = []
    for key, rows in grouped.items():
        by_class = {kind: sorted({row["title_key"] for row in rows if row["kind"] == "import_presence" and row["class"] == kind}) for kind in CLASSES}
        imports = sorted({row["title_key"] for row in rows if row["kind"] == "import_presence"})
        gaps = sorted(set().union(*(set(by_class[kind]) for kind in GAPS)))
        calls = sorted({row["title_key"] for row in rows if row["kind"] == "observed_unimplemented_call"})
        stops = sorted({row["title_key"] for row in rows if row["kind"] == "native_attributed_stop"})
        claims = sorted({row["title_key"] for row in rows if row["kind"] == "manually_reported_stop"})
        relevant = {row["title_key"] for row in rows}
        covered = relevant & set(runtime["covered_runtime_titles"])
        complete = relevant & set(runtime["complete_runtime_titles"])
        native = relevant & set(runtime["selected_runtime_titles"])
        manual = relevant & set(manual_titles)
        value = {"symbol": key[0], "names": sorted({row["name"] for row in rows if row["name"] is not None}), "by_class": by_class,
                 "classes": sorted(kind for kind in CLASSES if by_class[kind]), "imported_titles": imports, "imported_title_count": len(imports),
                 "gap_titles": gaps, "gap_title_count": len(gaps), "observed_call_titles": calls,
                 "observed_call_title_count": len(calls) if calls or covered else None,
                 "native_stop_titles": stops, "native_stop_title_count": len(stops) if stops or native else None,
                 "manual_stop_titles": claims, "manual_stop_title_count": len(claims) if manual else None,
                 "observation_ids": sorted(row["id"] for row in rows), "libraries": sorted({row["library"] for row in rows if row["library"] is not None}),
                 "provider_candidates": sorted({provider for row in rows for provider in row["providers"]}),
                 "evidence_kinds": sorted({kind for row in rows for kind in row["evidence_kinds"]}),
                 "provenance_status": "reported-build-unbound", "current_status": "unverified",
                 "runtime_coverage": "unavailable" if not covered else "lower-bound" if relevant - complete else "complete",
                 "runtime_denominators": {"relevant_titles": sorted(relevant), "covered_titles": sorted(covered), "complete_titles": sorted(complete),
                                          "unknown_titles": sorted(relevant - complete), "native_report_titles": sorted(native), "manual_claim_titles": sorted(manual)},
                 "next_actions": sorted_unique(item for row in rows for item in row["next_actions"])}
        if grouping == "library":
            value["library"] = key[1]
        if grouping == "provider":
            value["provider"] = key[1]
        result.append(value)
    return sorted(result, key=lambda row: (-row["gap_title_count"], -row["imported_title_count"], canonical(row)))


def build_census(manifests, max_input_bytes=67108864, allow_incomplete=False):
    inputs = Inputs(max_input_bytes)
    errors, entries, groups = [], [], defaultdict(list)
    units = {}

    def failure(source, error, key=None, code="invalid_input"):
        errors.append({"code": code, "source": locator(source), "message": str(error)})
        if key is not None:
            units[key] = "failed"

    for manifest in sorted({str(Path(path).absolute()) for path in manifests}):
        try:
            data, artifact = inputs.read({"path": manifest}, Path.cwd(), "anyps5-import-corpus-v1")
            document = decode_json(data)
            require(isinstance(document, dict) and document.get("format") == "anyps5-import-corpus" and type(document.get("version")) is int and document["version"] == 1, "manifest: unsupported format/version")
            require(not set(document) - {"format", "version", "titles"}, "manifest: unsupported fields")
            require(isinstance(document.get("titles"), list) and document["titles"], "manifest/titles: expected nonempty list")
        except (CensusError, OSError) as error:
            failure(manifest, error, "manifest:" + locator(manifest))
            continue
        for index, entry in enumerate(document["titles"]):
            try:
                title = title_metadata(entry)
                groups[title["key"]].append((title, entry, Path(manifest).parent, artifact, index))
            except CensusError as error:
                failure(locator(manifest) + f"#/titles/{index}", error, artifact + f":title:{index}")
    schema = None
    observations, titles, issues = [], [], []
    for title_key, specifications in sorted(groups.items()):
        title = copy.deepcopy(specifications[0][0])
        declarations = []
        conflict = False
        for candidate, _, _, artifact, index in specifications:
            declarations.append(evidence(artifact, f"/titles/{index}"))
            for field in ("target_platform", "content_version", "identity_source"):
                unknown = (None, "unknown") if field == "target_platform" else (None,)
                if title[field] in unknown:
                    title[field] = candidate[field]
                elif candidate[field] not in unknown and candidate[field] != title[field]:
                    failure(title_key, CensusError(f"conflicting selected {field}"), title_key + ":metadata", "selection_conflict")
                    conflict = True
        if conflict:
            continue
        title["declarations"] = sorted_unique(declarations)
        audits, native, claims = {}, defaultdict(dict), {}
        for _, entry, base, _, _ in sorted(specifications, key=lambda item: canonical(item[1])):
            selected = []
            if entry.get("audit") is not None:
                selected.append(("audit", entry["audit"]))
            selected += [("runtime", reference) for reference in entry.get("runtime_reports", [])]
            selected += [("claim", claim) for claim in entry.get("stop_claims", [])]
            for kind, reference in selected:
                unit = title_key + ":" + kind + ":" + canonical(reference)
                source = reference.get("log", {}).get("path", title_key) if kind == "claim" and isinstance(reference, dict) and isinstance(reference.get("log"), dict) else reference.get("path", title_key) if isinstance(reference, dict) else title_key
                try:
                    file_reference = reference.get("log") if kind == "claim" and isinstance(reference, dict) else reference
                    data, artifact = inputs.read(file_reference, base, {"audit": "legacy-import-audit", "runtime": "anyps5-report-v1", "claim": "checked-log-claim-v1"}[kind])
                    unit = title_key + ":" + kind + ":" + artifact
                    if kind == "audit":
                        document = decode_json(data)
                        validate_audit(document)
                        audits[artifact] = document
                    elif kind == "runtime":
                        require(len(data) <= 65536, "native report exceeds version-1 byte limit")
                        document = decode_json(data)
                        if schema is None:
                            schema = decode_json(SCHEMA.read_bytes())
                            schema_vocabulary(schema)
                        validate_schema(document, schema, schema)
                        runtime_association(title, document)
                        native[document["run"]["id"]][artifact] = document
                    else:
                        unit += ":" + fingerprint(canonical({key: value for key, value in reference.items() if key != "log"}).encode("utf-8"))
                        subject = validate_claim(reference, data)
                        claims[unit] = (reference, artifact, subject)
                    units[unit] = "accepted"
                except (CensusError, OSError, RecursionError, KeyError, TypeError) as error:
                    failure(source, error, unit)
        if len(audits) > 1:
            failure(title_key, CensusError("different audit bytes selected for one title; select exactly one: " + ", ".join(sorted(audits))), code="selection_conflict")
            for artifact in audits:
                units[title_key + ":audit:" + artifact] = "excluded"
        elif audits:
            artifact, document = next(iter(audits.items()))
            attach_imports(title, document, artifact, observations, issues)
        for run_id, snapshots in sorted(native.items()):
            if len(snapshots) > 1:
                failure(title_key, CensusError(f"different snapshots selected for run {run_id}; select exactly one"), code="selection_conflict")
                for artifact in snapshots:
                    units[title_key + ":runtime:" + artifact] = "excluded"
                continue
            artifact, document = next(iter(snapshots.items()))
            attach_runtime(title, document, artifact, observations)
        for reference, artifact, subject in claims.values():
            function = subject is not None and subject["kind"] in ("function", "function-name")
            supplied_symbol = subject.get("symbol") if function else None
            row = observation(title, "manually_reported_stop", evidence(artifact, line_start=reference["line_start"], line_end=reference["line_end"]),
                              symbol=supplied_symbol, name=subject.get("name") if subject else None,
                              library=subject.get("library") if function else None, subject=subject, label=reference.get("label"),
                              excerpt_sha256=fingerprint(reference["excerpt"].replace("\r\n", "\n").encode("utf-8")),
                              verification_method="caller-annotated-excerpt-checked", attribution="manual", run_id=None)
            row["next_actions"] = next_actions(row)
            observations.append(row)
        if title["audit"] is not None or title["runs"] or claims:
            title["runs"] = sorted(title["runs"], key=canonical)
            titles.append(title)
    accepted = sum(status == "accepted" for status in units.values())
    require(accepted > 0 and titles, "no usable selected evidence")
    errors = sorted_unique(errors)
    if errors and not allow_incomplete:
        raise CensusError("; ".join(error["source"] + ": " + error["message"] for error in errors))
    observations = sorted_unique(observations)
    runtime = runtime_coverage(titles)
    manual = {row["title_key"] for row in observations if row["kind"] == "manually_reported_stop"}
    runtime["manual_claim_titles"] = sorted(manual)
    shared_audits = defaultdict(list)
    for title in titles:
        if title["audit"] is not None:
            shared_audits[title["audit"]["artifact_id"]].append(title["key"])
    for artifact, shared in shared_audits.items():
        if len(shared) > 1:
            for title_key in shared:
                issues.append({"kind": "shared_audit_identity", "title_key": title_key, "shared_with": sorted(set(shared) - {title_key}),
                               "evidence": evidence(artifact, "/imports"), "next_actions": [action("verify_title_identity")]})
    result = {"format": "anyps5-import-census", "version": 1, "policy_version": 1, "status": "incomplete" if errors else "complete",
              "coverage": {"selected_titles": len(groups), "accepted_titles": len(titles), "selected_evidence": len(units), "accepted_evidence": accepted,
                           "failed_evidence": sum(status == "failed" for status in units.values()), "excluded_evidence": sum(status == "excluded" for status in units.values()),
                           "audited_titles": sum(title["audit"] is not None for title in titles), "runtime_titles": len(runtime["selected_runtime_titles"]),
                           "log_claim_titles": len({row["title_key"] for row in observations if row["kind"] == "manually_reported_stop"})},
              "artifacts": sorted(inputs.artifacts.values(), key=canonical), "titles": titles, "observations": observations,
              "runtime_coverage": runtime, "issues": sorted_unique(issues), "errors": errors,
              "next_actions": sorted_unique(item for row in observations for item in row["next_actions"])}
    result["symbols"] = summaries(observations, runtime, manual, "symbol")
    result["libraries"] = summaries(observations, runtime, manual, "library")
    result["providers"] = summaries(observations, runtime, manual, "provider")
    return result, inputs.paths


def validate_census(document):
    require(isinstance(document, dict) and document.get("format") == "anyps5-import-census" and type(document.get("version")) is int and document["version"] == 1
            and type(document.get("policy_version")) is int and document["policy_version"] == 1, "unsupported census format/version")
    allowed = {"format", "version", "policy_version", "status", "coverage", "artifacts", "titles", "observations", "runtime_coverage", "issues", "errors", "next_actions", "symbols", "libraries", "providers", "selection"}
    require(not set(document) - allowed, "census: unsupported fields")
    for field in ("artifacts", "titles", "observations", "symbols", "libraries", "providers", "issues", "errors", "next_actions"):
        require(isinstance(document.get(field), list), f"census/{field}: expected list")
    require(document.get("status") in ("complete", "incomplete") and isinstance(document.get("coverage"), dict), "invalid census status/coverage")
    coverage = document["coverage"]
    for field in ("selected_titles", "accepted_titles", "selected_evidence", "accepted_evidence", "failed_evidence", "excluded_evidence", "audited_titles", "runtime_titles", "log_claim_titles"):
        integer(coverage.get(field), "census/coverage/" + field)
    require(coverage["accepted_titles"] <= coverage["selected_titles"], "census: invalid title denominator")
    require(coverage["accepted_evidence"] + coverage["failed_evidence"] + coverage["excluded_evidence"] == coverage["selected_evidence"], "census: invalid evidence denominator")
    artifacts = {}
    for artifact in document["artifacts"]:
        require(isinstance(artifact, dict) and set(artifact) == {"id", "sha256", "bytes", "locators", "adapters"}, "census: invalid artifact")
        require(isinstance(artifact["sha256"], str) and re.fullmatch(r"[0-9a-f]{64}", artifact["sha256"]) and artifact["id"] == "sha256:" + artifact["sha256"], "census: invalid artifact digest")
        require(artifact["id"] not in artifacts, "census: duplicate artifact")
        integer(artifact["bytes"], "census/artifact/bytes")
        strings(artifact["locators"], "census/artifact/locators")
        require(all(locator(item) == item for item in artifact["locators"]), "census: artifact locator is not portable")
        strings(artifact["adapters"], "census/artifact/adapters")
        artifacts[artifact["id"]] = artifact

    def proof(value):
        require(isinstance(value, dict), "census: invalid evidence")
        require(not set(value) - {"artifact_id", "sha256", "pointer", "line_start", "line_end"}, "census: unsupported evidence fields")
        require(value.get("artifact_id") in artifacts and value.get("sha256") == artifacts[value["artifact_id"]]["sha256"], "census: unknown evidence artifact")
        if "pointer" in value:
            require(isinstance(value["pointer"], str) and value["pointer"].startswith("/"), "census: invalid JSON pointer")
            require("line_start" not in value and "line_end" not in value, "census: mixed evidence positions")
        else:
            integer(value.get("line_start"), "census/evidence/line_start", 1)
            integer(value.get("line_end"), "census/evidence/line_end", value["line_start"])

    title_keys = set()
    for item in document["titles"]:
        require(isinstance(item, dict), "census: invalid title")
        require(not set(item) - {"key", "title_id", "guest_platform", "target_platform", "content_version", "identity_source", "audit", "runs", "runtime_coverage", "build_match", "provenance_gaps", "declarations"}, "census: unsupported title fields")
        require(isinstance(item.get("title_id"), str) and re.fullmatch(r"[A-Z]{4}[0-9]{5}", item["title_id"]), "census: invalid title ID")
        require(item.get("guest_platform") in ("ps4", "ps5", "unknown") and item.get("key") == item["guest_platform"] + ":" + item["title_id"], "census: invalid title key")
        require(item["key"] not in title_keys, "census: duplicate title")
        title_keys.add(item["key"])
        require(item.get("target_platform") in ("windows", "linux", "macos", "unknown") and item.get("runtime_coverage") in ("unavailable", "complete", "lower-bound"), "census: invalid title platform/coverage")
        require(isinstance(item.get("runs"), list) and isinstance(item.get("declarations"), list), "census: invalid title evidence")
        for declaration in item["declarations"]:
            proof(declaration)
        require(item.get("build_match") is None, "census: unsupported build assertion")
        if item.get("audit") is not None:
            require(isinstance(item["audit"], dict) and item["audit"].get("artifact_id") in artifacts, "census: invalid audit association")
            require(all(item["audit"].get(field) is None for field in ("producer", "counting_policy", "provider_build")), "census: unsupported audit provenance assertion")
    observation_ids = set()
    kinds = {"import_presence", "observed_unimplemented_call", "native_attributed_stop", "manually_reported_stop", "unassigned_runtime_stop"}
    for row in document["observations"]:
        require(isinstance(row, dict) and row.get("kind") in kinds and row.get("title_key") in title_keys, "census: invalid observation")
        allowed = {"id", "title_key", "kind", "evidence_kinds", "symbol", "name", "library", "module", "providers", "evidence", "build_match", "current_status", "next_actions",
                   "class", "references", "weak", "name_verified", "library_mismatch", "symbol_mismatch", "unsearched_providers", "origin", "provider_status",
                   "calls", "run_id", "coverage", "lower_bound", "attribution", "stop", "verification_method", "subject", "label", "excerpt_sha256", "raw_symbol"}
        require(not set(row) - allowed, "census: unsupported observation fields")
        require(isinstance(row.get("id"), str) and re.fullmatch(r"observation:[0-9a-f]{64}", row["id"]) and row["id"] not in observation_ids, "census: invalid/duplicate observation ID")
        observation_ids.add(row["id"])
        for field in ("symbol", "name", "library", "module"):
            string(row.get(field), "census/observation/" + field, nullable=True, empty=field == "library")
        strings(row.get("providers"), "census/observation/providers")
        strings(row.get("evidence_kinds"), "census/observation/evidence_kinds")
        require(row.get("current_status") == "unverified" and row.get("build_match") is None, "census: unsupported observation provenance assertion")
        proof(row.get("evidence"))
        require(isinstance(row.get("next_actions"), list) and all(isinstance(item, dict) and isinstance(item.get("kind"), str) for item in row["next_actions"]), "census: invalid typed actions")
        if row["kind"] == "import_presence":
            require(row.get("class") in CLASSES and (row["symbol"] is not None or (row.get("raw_symbol") == "" and row.get("origin") == "module-file" and row["module"])), "census: invalid static class/symbol")
            integer(row.get("references"), "census/observation/references", 1)
        if row["kind"] == "observed_unimplemented_call":
            integer(row.get("calls"), "census/observation/calls", 1)
    require(isinstance(document.get("runtime_coverage"), dict), "census: missing runtime coverage")
    for field in ("selected_runtime_titles", "covered_runtime_titles", "complete_runtime_titles", "unknown_titles"):
        strings(document["runtime_coverage"].get(field), "census/runtime_coverage/" + field)
    strings(document["runtime_coverage"].get("manual_claim_titles"), "census/runtime_coverage/manual_claim_titles")
    for issue in document["issues"]:
        require(isinstance(issue, dict) and isinstance(issue.get("kind"), str) and issue.get("title_key") in title_keys, "census: invalid issue")
        require(not set(issue) - {"kind", "title_key", "symbol", "evidence", "next_actions", "provider", "library", "references", "declared_only", "importer", "chain", "name", "shared_with", "module"}, "census: unsupported issue fields")
        proof(issue.get("evidence"))
    for error in document["errors"]:
        require(isinstance(error, dict) and set(error) == {"code", "source", "message"} and all(isinstance(value, str) for value in error.values()), "census: invalid input error")
    require(document["status"] != "complete" or not document["errors"], "census: complete report contains input errors")
    if "selection" in document:
        selection = document["selection"]
        require(isinstance(selection, dict) and set(selection) == {"symbol", "title", "coverage_scope"} and selection["coverage_scope"] == "entire-corpus", "census: invalid selection scope")
        for field in ("symbol", "title"):
            string(selection[field], "census/selection/" + field, nullable=True)
    manual = set(document["runtime_coverage"]["manual_claim_titles"])
    for field, grouping in (("symbols", "symbol"), ("libraries", "library"), ("providers", "provider")):
        expected = summaries(document["observations"], document["runtime_coverage"], manual, grouping)
        require(canonical(document[field]) == canonical(expected), f"census/{field}: summary conservation failed")


def show_census(document, symbol=None, title=None):
    validate_census(document)
    result = copy.deepcopy(document)
    manual = set(document["runtime_coverage"]["manual_claim_titles"])
    if symbol is not None or title is not None:
        title_keys = {entry["key"] for entry in result["titles"] if title is None or entry["title_id"] == title or entry["key"] == title}
        query_symbols = {entry["symbol"] for entry in result["symbols"] if entry["symbol"] == symbol or symbol in entry["names"]} if symbol is not None else set()
        rows = [row for row in result["observations"] if row["title_key"] in title_keys and (symbol is None or row["symbol"] in query_symbols or row["name"] == symbol)]
        keys = {row["title_key"] for row in rows}
        if symbol is None:
            keys |= title_keys
        result["observations"] = rows
        result["titles"] = [entry for entry in result["titles"] if entry["key"] in keys]
        for field, grouping in (("symbols", "symbol"), ("libraries", "library"), ("providers", "provider")):
            result[field] = summaries(rows, result["runtime_coverage"], manual, grouping)
        result["issues"] = [entry for entry in result["issues"] if entry["title_key"] in keys and (symbol is None or entry.get("symbol") in query_symbols or entry.get("name") == symbol)]
        result["next_actions"] = sorted_unique(item for row in rows for item in row["next_actions"])
    result["selection"] = {"symbol": symbol, "title": title, "coverage_scope": "entire-corpus"}
    return result


def safe_text(value):
    if value is None:
        return "unknown"
    text = str(value)
    return "".join(character if character.isprintable() and character not in "|<>`&" else f"\\u{ord(character):04x}" for character in text)


def render(document):
    coverage = document["coverage"]
    lines = [f"{document['status']}: {coverage['accepted_titles']}/{coverage['selected_titles']} declared title IDs, {coverage['accepted_evidence']}/{coverage['selected_evidence']} evidence inputs accepted",
             "Classes are as reported; provider builds are unbound and current status is unverified.",
             "Symbol counts include only assigned evidence; unassigned native events and manual claims appear below.",
             "Symbol | Reported names | Imported titles | Reported gap titles | Titles with observed calls | Titles with native stops | Titles with manual stops"]
    for row in document["symbols"]:
        if row["gap_title_count"] or row["observed_call_titles"] or row["native_stop_titles"] or row["manual_stop_titles"]:
            lines.append(" | ".join(safe_text(value) for value in (row["symbol"], ", ".join(row["names"]), row["imported_title_count"], row["gap_title_count"], row["observed_call_title_count"], row["native_stop_title_count"], row["manual_stop_title_count"])))
    lines.append("Requested library | Symbol | Reported names | Absent titles | Stub titles | Partial titles | Reported provider candidates")
    for row in document["libraries"]:
        if row["gap_title_count"]:
            lines.append(" | ".join(safe_text(value) for value in (row["library"], row["symbol"], ", ".join(row["names"]), len(row["by_class"]["absent"]), len(row["by_class"]["stub"]), len(row["by_class"]["partial"]), ", ".join(row["provider_candidates"]))))
    for row in document["observations"]:
        if row["symbol"] is None:
            proof = row["evidence"]
            position = proof.get("pointer", f"lines {proof.get('line_start')}-{proof.get('line_end')}")
            lines.append(f"Unassigned {safe_text(row['kind'])}: {safe_text(row['title_key'])} {safe_text(row['name'] or row.get('label') or row['module'])} [{proof['artifact_id']} {safe_text(position)}]")
    if "selection" in document:
        for row in document["observations"]:
            lines.append(f"{safe_text(row['kind'])} {safe_text(row['title_key'])}: symbol={safe_text(row['symbol'])}; reported name={safe_text(row['name'])}; library={safe_text(row['library'])}; providers={safe_text(', '.join(row['providers']))}; evidence={safe_text(canonical(row['evidence']))}")
            lines.append("  Next: " + ", ".join(item["kind"] for item in row["next_actions"]))
    lines.append(f"{len(document['issues'])} loader/package issues; {len(document['errors'])} input errors.")
    for issue in document["issues"]:
        details = issue.get("library") or issue.get("provider") or issue.get("symbol") or issue.get("module") or " -> ".join(issue.get("chain", []))
        lines.append("  " + safe_text(issue["kind"]) + " " + safe_text(issue["title_key"]) + ": " + safe_text(details))
    for error in document["errors"]:
        lines.append("  " + safe_text(error["source"]) + ": " + safe_text(error["message"]))
    return "\n".join(lines)


def write_output(path, data, inputs):
    output = Path(path)
    try:
        require(not network_path(path), "JSON output UNC paths are unsupported")
        resolved = output.resolve()
        require(not network_path(resolved), "JSON output resolved UNC path is unsupported")
        require(not output.is_dir(), "JSON output is a directory")
        for source in inputs:
            require(resolved != source.resolve(), "JSON output would overwrite an input file")
            if output.exists() and source.exists():
                require(not output.samefile(source), "JSON output would overwrite an input file alias")
        require(output.parent.is_dir(), "JSON output parent is not a directory")
    except (OSError, RuntimeError) as error:
        raise CensusError("cannot check JSON output against inputs") from error
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(mode="wb", prefix=".import-census-", suffix=".tmp", dir=output.parent, delete=False) as handle:
            temporary = Path(handle.name)
            handle.write(data)
            handle.flush()
            os.fsync(handle.fileno())
        os.replace(temporary, output)
    except OSError as error:
        raise CensusError(f"cannot write JSON output ({error.strerror or type(error).__name__})") from error
    finally:
        if temporary is not None:
            try:
                temporary.unlink(missing_ok=True)
            except OSError:
                pass


def positive(value):
    try:
        result = int(value)
    except ValueError as error:
        raise argparse.ArgumentTypeError("expected positive integer") from error
    if result < 1:
        raise argparse.ArgumentTypeError("expected positive integer")
    return result


def main(argv=None):
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, "reconfigure"):
            stream.reconfigure(encoding="utf-8", errors="backslashreplace")
    parser = argparse.ArgumentParser(description="Correlate reported title imports, observed unimplemented calls and reported stops without running titles or accessing the network.")
    commands = parser.add_subparsers(dest="command", required=True)
    build = commands.add_parser("build", help="build a census from explicitly selected local evidence")
    build.add_argument("manifests", nargs="+", type=Path)
    build.add_argument("--json", type=Path, help="atomically write the census JSON")
    build.add_argument("--allow-incomplete", action="store_true", help="write failures and qualified partial evidence, still exiting 2")
    show = commands.add_parser("show", help="inspect a saved census and evidence positions")
    show.add_argument("census", type=Path)
    show.add_argument("--symbol")
    show.add_argument("--title")
    for command in (build, show):
        command.add_argument("--format", choices=("table", "json"), default="table")
        command.add_argument("--max-input-bytes", type=positive, default=67108864)
        command.add_argument("--max-output-bytes", type=positive, default=268435456)
    show.set_defaults(max_input_bytes=268435456)
    args = parser.parse_args(argv)
    try:
        if args.command == "build":
            document, inputs = build_census(args.manifests, args.max_input_bytes, args.allow_incomplete)
        else:
            reader = Inputs(args.max_input_bytes)
            data, _ = reader.read({"path": str(args.census)}, Path.cwd(), "anyps5-import-census-v1")
            document = show_census(decode_json(data), args.symbol, args.title)
            inputs = reader.paths
        data = (canonical(document) + "\n").encode("utf-8")
        require(len(data) <= args.max_output_bytes, "report exceeds --max-output-bytes")
        if args.command == "build" and args.json:
            write_output(args.json, data, inputs)
        print(data.decode("utf-8"), end="") if args.format == "json" else print(render(document))
        return 2 if document["status"] == "incomplete" else 0
    except (CensusError, OSError, ValueError, RecursionError, KeyError, TypeError) as error:
        print("FAIL: " + safe_text(error), file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
