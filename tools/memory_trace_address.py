import argparse
import re
import sys
from dataclasses import dataclass
from pathlib import Path


TRACE_PREFIX = re.compile(r"^\[memory\]\s+")
RANGE = re.compile(r"(?P<address>(?:0x)?[0-9a-fA-F]+)\+(?P<size>0x[0-9a-fA-F]+)")
OPERATIONS = (
    "remap fixed",
    "map direct",
    "map anon",
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
    events = []
    for number, line in enumerate(lines, 1):
        match = TRACE_PREFIX.match(line)
        if match is None:
            continue
        detail = line[match.end():].rstrip("\r\n")
        operation = next((item for item in OPERATIONS if detail.startswith(item + " ")), None)
        if operation is None:
            continue
        address = RANGE.search(detail)
        if address is None:
            raise ValueError(f"line {number}: {operation} event has no address range")
        events.append(MappingEvent(number, operation, int(address["address"], 16),
                                   int(address["size"], 16), detail))
    return events


def parse_range(address, size):
    try:
        start = int(address, 0)
        length = int(size, 0)
    except ValueError as error:
        raise ValueError("address and byte count must be decimal or 0x-prefixed integers") from error
    if start < 0 or start >= 1 << 64:
        raise ValueError("address must be an unsigned 64-bit value")
    if length <= 0 or length >= 1 << 64 or start + length > 1 << 64:
        raise ValueError("byte count must be positive and the queried range must fit in 64 bits")
    return start, length


def overlaps(event, start, size):
    return event.address < start + size and start < event.address + event.size


def select_events(events, start, size):
    return [event for event in events if overlaps(event, start, size)]


def render_events(events, path, start, size, limit):
    matches = select_events(events, start, size)
    lines = [f"APS5_TRACE_MEMORY events overlapping 0x{start:x}+0x{size:x} in {path}"]
    if not matches:
        lines.append("No traced mapping operations overlap this range.")
        return "\n".join(lines) + "\n"
    if len(matches) > limit:
        lines.append(f"Showing the last {limit} of {len(matches)} matching events.")
        matches = matches[-limit:]
    lines.extend(f"line {event.line}: {event.detail}" for event in matches)
    return "\n".join(lines) + "\n"


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
        with args.trace.open(encoding="utf-8", errors="replace") as source:
            events = parse_events(source)
        sys.stdout.write(render_events(events, args.trace, start, size, args.limit))
    except (OSError, ValueError) as error:
        parser.error(str(error))
    return 0


if __name__ == "__main__":
    sys.exit(main())
