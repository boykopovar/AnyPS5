import argparse
import codecs
import re
import sys
from collections import deque
from dataclasses import dataclass
from io import TextIOWrapper
from pathlib import Path


TRACE_PREFIX = re.compile(r"^\[memory\]\s+")
RANGE = re.compile(r"\s*(?P<address>(?:0[xX])?[0-9a-fA-F]+|\(nil\))\+(?P<size>0[xX][0-9a-fA-F]+)(?=\s|$)")
ADDRESS_LIMIT = 1 << 64
OPERATIONS = (
    "remap fixed",
    "map direct",
    "map anon",
    "flexible backing",
    "map file",
    "map shared",
    "protect",
    "unmap released view",
    "unmap",
    "reserve",
)


@dataclass(frozen=True)
class MappingEvent:
    line: int
    operation: str
    address: int
    size: int
    detail: str


def parse_events(lines):
    return list(iter_events(lines))


def iter_events(lines):
    for number, line in enumerate(lines, 1):
        match = TRACE_PREFIX.match(line)
        if match is None:
            continue
        detail = line[match.end():].rstrip("\r\n")
        operation = next((item for item in OPERATIONS if detail == item or detail.startswith(item + " ")), None)
        if operation is None:
            continue
        address = RANGE.match(detail, len(operation) + 1)
        if address is None:
            raise ValueError(f"line {number}: {operation} event has no address range")
        start = 0 if address["address"] == "(nil)" else int(address["address"], 16)
        size = int(address["size"], 16)
        if start >= ADDRESS_LIMIT or size >= ADDRESS_LIMIT or start + size > ADDRESS_LIMIT:
            raise ValueError(f"line {number}: {operation} event range must fit in unsigned 64-bit addresses")
        yield MappingEvent(number, operation, start, size, detail)


def parse_number(value):
    if value.strip().lstrip("+-").lower().startswith("0x"):
        return int(value, 16)
    return int(value, 10)


def parse_range(address, size):
    try:
        start = parse_number(address)
        length = parse_number(size)
    except ValueError as error:
        raise ValueError("address and byte count must be decimal or 0x-prefixed integers") from error
    if not 0 <= start < ADDRESS_LIMIT:
        raise ValueError("address must be an unsigned 64-bit value")
    if not 0 < length < ADDRESS_LIMIT or start + length > ADDRESS_LIMIT:
        raise ValueError("byte count must be positive and the queried range must fit in 64 bits")
    return start, length


def overlaps(event, start, size):
    return event.size > 0 and event.address < start + size and start < event.address + event.size


def select_events(events, start, size):
    return [event for event in events if overlaps(event, start, size)]


def render_events(events, path, start, size, limit):
    matches = deque()
    count = 0
    for event in events:
        if overlaps(event, start, size):
            count += 1
            matches.append(event)
            if len(matches) > limit:
                matches.popleft()
    lines = [f"APS5_TRACE_MEMORY events overlapping 0x{start:x}+0x{size:x} in {path}"]
    if not matches:
        lines.append("No traced mapping operations overlap this range.")
        return "\n".join(lines) + "\n"
    if count > limit:
        lines.append(f"Showing the last {limit} of {count} matching events.")
    lines.extend(f"line {event.line}: {event.detail}" for event in matches)
    return "\n".join(lines) + "\n"


def decode_trace(source):
    bom = source.peek(4)[:4]
    if bom in (codecs.BOM_UTF32_BE, codecs.BOM_UTF32_LE):
        encoding = "utf-32"
    elif bom[:2] in (codecs.BOM_UTF16_BE, codecs.BOM_UTF16_LE):
        encoding = "utf-16"
    else:
        encoding = "utf-8-sig"
    return TextIOWrapper(source, encoding=encoding, errors="replace")


def main(argv=None):
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, "reconfigure"):
            stream.reconfigure(encoding="utf-8", errors="backslashreplace")
    parser = argparse.ArgumentParser(
        description="show APS5_TRACE_MEMORY mapping operations overlapping a guest address range")
    parser.add_argument("trace", type=Path, help="file containing APS5_TRACE_MEMORY stderr output")
    parser.add_argument("--address", required=True, help="guest address (decimal or 0x-prefixed)")
    parser.add_argument("--length", default="1", help="queried byte count (default: 1)")
    parser.add_argument("--limit", type=int, default=40, help="maximum matching events to print (default: 40)")
    args = parser.parse_args(argv)
    if args.limit < 1:
        parser.error("--limit must be positive")
    try:
        start, size = parse_range(args.address, args.length)
        with args.trace.open("rb") as raw, decode_trace(raw) as source:
            output = render_events(iter_events(source), args.trace, start, size, args.limit)
        sys.stdout.write(output)
    except (OSError, ValueError) as error:
        parser.error(str(error))
    return 0


if __name__ == "__main__":
    sys.exit(main())
