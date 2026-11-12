import argparse
import json
import re
import sys
from pathlib import Path


OFFSET = re.compile(r"0x[0-9a-f]+\Z")


def _count(value, location):
    if isinstance(value, bool) or not isinstance(value, int) or value < 0:
        raise ValueError(f"{location}: expected a non-negative integer")
    return value


def _image(value, location, needs_address):
    if not isinstance(value, dict):
        raise ValueError(f"{location}: expected an object")
    module, offset = value.get("module"), value.get("offset")
    if module is not None and (not isinstance(module, str) or not module):
        raise ValueError(f"{location}.module: expected a non-empty string or null")
    if offset is not None and (not isinstance(offset, str) or not OFFSET.fullmatch(offset)):
        raise ValueError(f"{location}.offset: expected a lowercase hexadecimal offset or null")
    if (module is None) != (offset is None):
        raise ValueError(f"{location}: module and offset must both be resolved or both be null")
    result = {"module": module, "offset": offset}
    if needs_address:
        address = value.get("address")
        if not isinstance(address, str) or not OFFSET.fullmatch(address):
            raise ValueError(f"{location}.address: expected a lowercase hexadecimal address")
        result["address"] = address
    return result


def load_graph(path):
    try:
        value = json.loads(Path(path).read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError) as error:
        raise ValueError(f"cannot read graph {path}: {error}") from error
    if not isinstance(value, dict):
        raise ValueError(f"{path}: expected a graph object")
    version = value.get("version")
    if isinstance(version, bool) or not isinstance(version, int) or version != 1:
        raise ValueError(f"{path}.version: unsupported graph format")
    total_calls = _count(value.get("total_calls"), f"{path}.total_calls")
    dropped = _count(value.get("dropped_callsite_events"), f"{path}.dropped_callsite_events")
    edges = value.get("edges")
    if not isinstance(edges, list):
        raise ValueError(f"{path}.edges: expected an array")
    aggregate = {}
    for index, edge in enumerate(edges):
        location = f"{path}.edges[{index}]"
        if not isinstance(edge, dict):
            raise ValueError(f"{location}: expected an object")
        calls = _count(edge.get("calls"), f"{location}.calls")
        if calls == 0:
            raise ValueError(f"{location}.calls: expected a positive count")
        consumer = _image(edge.get("consumer"), f"{location}.consumer", True)
        imported = edge.get("import")
        if not isinstance(imported, dict):
            raise ValueError(f"{location}.import: expected an object")
        library, name = imported.get("library"), imported.get("name")
        if not isinstance(library, str) or not isinstance(name, str) or not name:
            raise ValueError(f"{location}.import: expected string library and non-empty name fields")
        provider = _image(edge.get("provider"), f"{location}.provider", True)
        key = (consumer["module"], consumer["offset"], consumer["address"], library, name,
               provider["module"], provider["offset"], provider["address"])
        if key not in aggregate:
            aggregate[key] = {"calls": 0, "consumer": consumer, "import": {"library": library, "name": name},
                              "provider": provider}
        aggregate[key]["calls"] += calls
    ordered = sorted(aggregate.values(), key=lambda edge: (
        -edge["calls"], edge["consumer"]["module"] or "", edge["consumer"]["offset"] or "",
        edge["import"]["library"], edge["import"]["name"], edge["provider"]["module"] or "",
        edge["provider"]["offset"] or "", edge["consumer"]["address"]))
    return {"version": 1, "total_calls": total_calls, "dropped_callsite_events": dropped, "edges": ordered}


def _image_label(image):
    if image["module"] is None:
        return "<unresolved> (" + image.get("address", "") + ")"
    return image["module"] + "+" + image["offset"]


def _cell(value):
    return " ".join(str(value).split()).replace("|", r"\|")


def report(graph, limit=None, consumer=None, provider=None, library=None, unresolved_only=False):
    edges = [edge for edge in graph["edges"]
             if (consumer is None or edge["consumer"]["module"] == consumer)
             and (provider is None or edge["provider"]["module"] == provider)
             and (library is None or edge["import"]["library"] == library)
             and (not unresolved_only or edge["consumer"]["module"] is None or edge["provider"]["module"] is None)]
    if limit is not None:
        edges = edges[:limit]
    calls = sum(edge["calls"] for edge in edges)
    lines = ["# Runtime import provider graph", "",
             f"{calls} calls across {len(edges)} shown edges; {graph['dropped_callsite_events']} callsite events were not recorded.", "",
             "| Calls | Consumer | Import | Provider |", "| ---: | --- | --- | --- |"]
    for edge in edges:
        imported = edge["import"]
        import_label = imported["library"] + " / " + imported["name"]
        lines.append("| " + str(edge["calls"]) + " | " + _cell(_image_label(edge["consumer"])) + " | " +
                     _cell(import_label) + " | " + _cell(_image_label(edge["provider"])) + " |")
    return "\n".join(lines) + "\n"


def main(argv=None):
    parser = argparse.ArgumentParser(description="summarize actual imported-function providers observed by APS5_TRACE_CALLS")
    parser.add_argument("graph", type=Path, help="JSON written through APS5_TRACE_CALLS_GRAPH_FILE")
    parser.add_argument("--consumer", help="keep edges from this loaded consumer module")
    parser.add_argument("--provider", help="keep edges resolved in this host provider module")
    parser.add_argument("--library", help="keep imports from this guest library")
    parser.add_argument("--limit", type=int, help="show at most this many edges")
    parser.add_argument("--unresolved", action="store_true", help="show edges with an unresolved consumer or provider image")
    parser.add_argument("--json", action="store_true", help="write the validated, aggregated graph as JSON")
    args = parser.parse_args(argv)
    if args.limit is not None and args.limit < 0:
        parser.error("--limit must be non-negative")
    try:
        graph = load_graph(args.graph)
        if args.json:
            sys.stdout.write(json.dumps(graph, ensure_ascii=False, indent=2) + "\n")
        else:
            sys.stdout.write(report(graph, args.limit, args.consumer, args.provider, args.library, args.unresolved))
    except (OSError, ValueError) as error:
        print(f"runtime_call_graph: {error}", file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
